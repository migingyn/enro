#include "calls/WakeLadder.hpp"

#include "app/App.hpp"
#include "models/Json.hpp"
#include "planner/DayPlanner.hpp"
#include "util/Log.hpp"
#include "util/Time.hpp"

namespace enro {

namespace {

constexpr int kStepMinutes = 3;
constexpr int kVerifyMinutes = 5;
constexpr long long kDemoRetrySeconds = 3;

std::string ladderKey(const std::string& user_id) { return "ladder:" + user_id; }

const char* toneFor(int attempt) { return attempt >= 3 ? "yelling" : attempt == 2 ? "louder" : "calm"; }

}  // namespace

// ---------- state ----------

LadderState WakeLadder::load(const std::string& user_id) const {
  LadderState s;
  auto raw = ctx_.repos->kv(ladderKey(user_id));
  if (!raw) return s;
  auto j = nlohmann::json::parse(*raw);
  s.ladder_id = j.value("ladder_id", "");
  s.next_attempt = j.value("next_attempt", 1);
  s.awake = j.value("awake", false);
  s.demo = j.value("demo", false);
  s.gap_s = j.value("gap_s", 180LL);
  s.last_call_id = j.value("last_call_id", "");
  return s;
}

void WakeLadder::save(const std::string& user_id, const LadderState& s) {
  ctx_.repos->setKv(ladderKey(user_id), nlohmann::json{{"ladder_id", s.ladder_id},
                                                        {"next_attempt", s.next_attempt},
                                                        {"awake", s.awake},
                                                        {"demo", s.demo},
                                                        {"gap_s", s.gap_s},
                                                        {"last_call_id", s.last_call_id}}
                                                       .dump());
}

std::string WakeLadder::lastCallId(const std::string& user_id) const { return load(user_id).last_call_id; }

void WakeLadder::queueAttempt(const std::string& user_id, const LadderState& s, int attempt, long long due_at) {
  ctx_.queue->push({due_at, user_id, TaskKind::WAKE_CALL, {{"ladder_id", s.ladder_id}, {"attempt", attempt}}});
  ctx_.scheduler->wake();
}

// ---------- plan ----------

WakePlan WakeLadder::plan(const User& user, const std::string& date) const {
  WakePlan w;
  w.alarm_time = user.alarm_time.empty() ? "07:00" : user.alarm_time;
  w.alarm_ts = time::atLocalTime(date, w.alarm_time, user.timezone.empty() ? "America/Los_Angeles" : user.timezone);
  w.grace_minutes = user.grace_minutes;
  long long call1 = w.alarm_ts + user.grace_minutes * 60LL;
  for (int attempt = 1; attempt <= 3; ++attempt) {
    w.steps.push_back({"CALL_" + std::to_string(attempt), call1 + (attempt - 1) * kStepMinutes * 60LL, toneFor(attempt),
                       attempt == 3});
  }
  w.steps.push_back({"AWAKE_VERIFY", w.steps.back().at + kVerifyMinutes * 60LL, "check", false});
  return w;
}

// ---------- starting a ladder ----------

std::string WakeLadder::start(const std::string& user_id) {
  LadderState s;
  s.ladder_id = "ladder_" + std::to_string(time::nowMs());
  s.demo = true;
  s.gap_s = ctx_.env.getInt("LADDER_DEMO_GAP_SECONDS", 20);
  s.last_call_id = callNow(user_id, "wake", 1);
  s.next_attempt = 2;
  save(user_id, s);
  queueAttempt(user_id, s, 2, time::nowTs() + s.gap_s);
  LOG_INFO("ladder", "demo ladder " << s.ladder_id << " for " << user_id << ", " << s.gap_s << " s between calls");
  return s.last_call_id;
}

std::string WakeLadder::schedule(const std::string& user_id, const std::string& date) {
  auto user = ctx_.repos->user(user_id);
  if (!user) return "";
  WakePlan w = plan(*user, date);
  LadderState s;
  s.ladder_id = "ladder_" + user_id + "_" + date;
  s.gap_s = kStepMinutes * 60;
  save(user_id, s);
  queueAttempt(user_id, s, 1, w.steps.front().at);
  LOG_INFO("ladder", "scheduled " << s.ladder_id << ", CALL_1 at " << time::hhmmIn(w.steps.front().at, user->timezone));
  return s.ladder_id;
}

// ---------- the step handler ----------

void WakeLadder::registerHandlers(Scheduler& scheduler) {
  scheduler.on(TaskKind::WAKE_CALL, [this](const AgentTask& t) { handleWakeCall(t); });
}

void WakeLadder::handleWakeCall(const AgentTask& task) {
  const std::string& user_id = task.user_id;
  LadderState s = load(user_id);
  if (task.payload.value("ladder_id", "") != s.ladder_id) return;  // an older ladder

  if (task.payload.value("purpose", "") == "awake_verify") {
    s.last_call_id = callNow(user_id, "awake_verify", 1);
    save(user_id, s);
    return;
  }

  int attempt = task.payload.value("attempt", 1);
  if (s.awake || attempt != s.next_attempt || attempt > 3) return;  // done, or already pulled forward

  // Still talking to the previous call: wait instead of ringing over it.
  if (auto last = ctx_.repos->call(s.last_call_id); last && last->state == "in_progress") {
    queueAttempt(user_id, s, attempt, task.due_at + s.gap_s);
    return;
  }

  nlohmann::json input = {{"attempt", attempt}, {"answered", false}, {"awake", false},
                          {"ladder_id", s.ladder_id}};
  ReflexVerdict v = ctx_.reflex->stopCondition(input);
  if (attempt == 3) v.verdict = "ESCALATE";  // PRD 7.6: CALL_3 always brings in emergency contacts
  ctx_.recordDecision(user_id, "stop_condition", v, input);
  if (v.verdict == "STOP") return;
  if (v.verdict == "BACK_OFF") {
    queueAttempt(user_id, s, attempt, task.due_at + s.gap_s);
    return;
  }

  s.last_call_id = callNow(user_id, "wake", attempt);
  s.next_attempt = attempt + 1;
  save(user_id, s);

  if (attempt == 3) {
    std::string hint = firstThingHint(user_id);
    int sent = ctx_.contacts->notifyAll(user_id, hint);
    LOG_INFO("ladder", "CALL_3 for " << user_id << ", texted " << sent << " emergency contact(s)");
  } else {
    queueAttempt(user_id, s, attempt + 1, task.due_at + s.gap_s);
  }
}

// ---------- events from the conversation ----------

void WakeLadder::onNoAnswer(const std::string& call_id) {
  auto call = ctx_.repos->call(call_id);
  if (!call || call->purpose != "wake") return;
  LadderState s = load(call->user_id);
  if (s.awake || call_id != s.last_call_id || s.next_attempt > 3) return;
  if (s.demo) {
    // Demo: a declined call escalates in seconds. Real mornings keep the 3 minute steps.
    queueAttempt(call->user_id, s, s.next_attempt, time::nowTs() + kDemoRetrySeconds);
  }
  LOG_INFO("ladder", call_id << " not answered, next is CALL_" << s.next_attempt);
}

void WakeLadder::onAwake(const std::string& call_id) {
  auto call = ctx_.repos->call(call_id);
  if (!call) return;
  LadderState s = load(call->user_id);
  if (s.awake) return;
  s.awake = true;
  save(call->user_id, s);
  nlohmann::json input = {{"attempt", call->attempt}, {"answered", true}, {"awake", true}, {"ladder_id", s.ladder_id}};
  ctx_.recordDecision(call->user_id, "stop_condition", {"STOP", 1.0, "rules", 0}, input);
  ctx_.queue->push({time::nowTs() + kVerifyMinutes * 60LL, call->user_id, TaskKind::WAKE_CALL,
                    {{"ladder_id", s.ladder_id}, {"purpose", "awake_verify"}}});
  ctx_.scheduler->wake();
  LOG_INFO("ladder", call->user_id << " is awake, verification call in " << kVerifyMinutes << " min");
}

// ---------- helpers ----------

std::string WakeLadder::firstThingHint(const std::string& user_id) const {
  // Same day the call opener talks about (DayPlanner picks today, or the demo day in DEMO_MODE).
  DayPlan day = DayPlanner(ctx_).build(user_id);
  for (auto& e : day.events) {
    if (e.canceled) continue;
    std::string hhmm = time::hhmmIn(e.start_ts, day.timezone);
    return hhmm[0] == '0' ? hhmm.substr(1) : hhmm;  // "8:00", not "08:00"
  }
  return "";
}

std::string WakeLadder::callNow(const std::string& user_id, const std::string& purpose, int attempt) {
  auto user = ctx_.repos->user(user_id);
  Call call;
  call.id = "call_" + std::to_string(time::nowMs());
  // Two calls in the same millisecond (tests, fast ticks) must not share an id.
  while (ctx_.repos->call(call.id)) call.id += "x";
  call.user_id = user_id;
  call.purpose = purpose;
  call.state = "ringing";
  call.attempt = attempt;
  call.turns = 0;

  CallRequest req;
  req.call_id = call.id;
  req.user_id = user_id;
  req.to_phone = user ? user->phone : "";
  req.purpose = purpose;
  req.attempt = attempt;
  req.persona = user ? user->archetype_id : "best_friend";
  req.urgency = attempt >= 3 ? Urgency::URGENT : attempt == 2 ? Urgency::FIRM : Urgency::CHILL;
  call.provider_sid = ctx_.voice->placeCall(req);
  ctx_.updateCall(call);
  return call.id;
}

}  // namespace enro
