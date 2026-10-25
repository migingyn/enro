#include "calls/Conversation.hpp"

#include <algorithm>
#include <cmath>

#include "app/App.hpp"
#include "models/Json.hpp"
#include "planner/DayPlanner.hpp"
#include "util/Time.hpp"

namespace enro {

namespace {

std::string countWord(int n) {
  static const char* words[] = {"zero", "one", "two",    "three",    "four",     "five",    "six",    "seven",
                                "eight", "nine", "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen"};
  return n >= 0 && n <= 15 ? words[n] : std::to_string(n);
}

std::string minutesWord(int n) { return countWord(n) + (n == 1 ? " minute" : " minutes"); }

// The event the user is getting up for: the first one on the plan that hasn't been canceled.
const Event* firstEvent(const DayPlan& day) {
  for (auto& e : day.events)
    if (!e.canceled) return &e;
  return nullptr;
}

}  // namespace

AgentLine Conversation::say(const Call& call, const std::string& purpose, const nlohmann::json& facts,
                            const std::string& fallback) {
  auto user = ctx_.repos->user(call.user_id);
  LineRequest req;
  req.purpose = purpose;
  req.persona = user ? user->archetype_id : "best_friend";
  req.urgency = call.attempt >= 3 ? Urgency::URGENT : call.attempt == 2 ? Urgency::FIRM : Urgency::CHILL;
  req.user_name = user ? user->name : "";
  req.facts = facts;
  // Without an LLM, canned lines can't carry the real numbers, so the fallback (built from them) wins.
  bool keyless = ctx_.llm->name() == "fake";
  std::string text = keyless && !fallback.empty() ? fallback : ctx_.lines->generate(req);
  AgentLine line{text, req.persona, req.urgency, false};
  ctx_.recordTurn({call.id, "agent", line.text, "", time::nowTs()});
  return line;
}

SnoozeDecision Conversation::negotiate(const std::string& user_id, int requested) {
  DayPlanner planner(ctx_);
  DayPlan day = planner.build(user_id);
  const Event* first = firstEvent(day);

  SnoozeDecision d;
  d.requested = std::max(1, requested);
  PunctualityStats stats = ctx_.punctuality->forEventType(first ? first->event_type : "class");
  d.history_samples = stats.samples;
  d.inflation = stats.samples > 0 && stats.snooze_inflation > 0 ? stats.snooze_inflation : 1.0;
  d.effective = static_cast<int>(std::lround(d.requested * d.inflation));
  // Same request as the last three times: quote what it really turned into ("you said five, it was eleven").
  auto recent = ctx_.punctuality->recentSnoozes(first ? first->event_type : "class");
  if (recent.size() >= 3) {
    auto tail = std::vector<SnoozeSample>(recent.end() - 3, recent.end());
    if (std::all_of(tail.begin(), tail.end(), [&](auto& s) { return s.requested_minutes == d.requested; })) {
      int sum = 0;
      for (auto& s : tail) sum += s.actual_minutes;
      d.effective = static_cast<int>(std::lround(sum / 3.0));
      d.history_samples = 3;
      d.repeat_request = true;
    }
  }
  d.consequences = ctx_.consequences->evaluate(user_id, day.events, day.plans, d.effective);

  // Largest snooze that stays on time once inflated, checked minute by minute.
  for (int m = d.requested - 1; m >= 1; --m) {
    int eff = static_cast<int>(std::lround(m * d.inflation));
    if (ctx_.consequences->evaluate(user_id, day.events, day.plans, eff).worst == ConsequenceStatus::OK) {
      d.counter = m;
      break;
    }
  }

  // The consequences decide by default: OK -> GRANT, AT_RISK -> COUNTER, MISSED -> REFUSE.
  nlohmann::json inputs = {{"requested_minutes", d.requested},
                           {"effective_minutes", d.effective},
                           {"snooze_inflation", d.inflation},
                           {"consequences", d.consequences}};
  ReflexVerdict v = ctx_.reflex->snooze(inputs);
  std::string policy = d.consequences.worst == ConsequenceStatus::OK      ? "GRANT"
                       : d.consequences.worst == ConsequenceStatus::AT_RISK ? "COUNTER"
                                                                           : "REFUSE";
  // A confident reflex verdict wins. A low-confidence one (the rules placeholder) defers to the engine.
  if (v.confidence < 0.6 || (v.verdict != "GRANT" && v.verdict != "COUNTER" && v.verdict != "REFUSE")) {
    v.verdict = policy;
    v.confidence = 0.9;
  }
  if (v.verdict == "COUNTER" && d.counter == 0) v.verdict = "REFUSE";
  d.verdict = v.verdict;
  ctx_.recordDecision(user_id, "snooze", v, inputs);
  return d;
}

std::optional<AgentLine> Conversation::onEvent(const std::string& call_id, const std::string& event,
                                               const std::string& text) {
  auto call = ctx_.repos->call(call_id);
  if (!call) return std::nullopt;

  if (event == "answered") {
    call->state = "in_progress";
    ctx_.updateCall(*call);
    stage_[call_id] = "opened";

    auto user = ctx_.repos->user(call->user_id);
    std::string name = user && !user->name.empty() ? user->name.substr(0, user->name.find(' ')) : "hey";
    DayPlanner planner(ctx_);
    DayPlan day = planner.build(call->user_id);
    const Event* first = firstEvent(day);
    std::string event_title = first ? first->title : "your first thing";
    std::string time_str = first ? time::hhmmIn(first->start_ts, day.timezone) : "";

    std::string purpose = call->purpose == "wake" ? "wake_call_" + std::to_string(std::min(call->attempt, 3)) : call->purpose;
    nlohmann::json facts = {{"event", event_title}, {"minutes", 10}, {"time", time_str}};
    std::string fallback;
    if (call->purpose == "leave_reminder" && first) {
      auto it = std::find_if(day.plans.begin(), day.plans.end(), [&](auto& p) { return p.event_id == first->id; });
      std::string leave = it != day.plans.end() ? time::hhmmIn(it->leave_by, day.timezone) : time_str;
      fallback = name + ", leave by " + leave + " for " + event_title + ". Are you heading out?";
    } else {
      // Opener with consequences: what ten more minutes of sleep would cost.
      ConsequenceReport ten = ctx_.consequences->evaluate(call->user_id, day.events, day.plans, 10);
      facts["consequences"] = ten;
      std::string opener = call->attempt >= 3   ? name + "! Get up! This is call three."
                           : call->attempt == 2 ? name + ", second call. Up you get."
                                                : "Morning " + name + ", it's Enro.";
      fallback = opener + (first ? " " + event_title + " is at " + time_str + ". " + ten.summary : "") +
                 " What's your first thing today?";
    }
    return say(*call, purpose, facts, fallback);
  }

  if (event == "speech") {
    Intent intent = ctx_.intents->classify(text, {{"purpose", call->purpose}, {"attempt", call->attempt}});
    ctx_.recordTurn({call_id, "user", text, intent.label, time::nowTs()});
    call->turns += 1;
    ctx_.updateCall(*call);

    if (intent.label == "SNOOZE_REQUEST") {
      SnoozeDecision d = negotiate(call->user_id, intent.minutes);
      nlohmann::json facts = {{"minutes", d.requested}, {"requested", d.requested}, {"effective", d.effective},
                              {"counter", d.counter},   {"inflation", d.inflation}, {"consequences", d.consequences}};
      std::string history;
      if (d.effective > d.requested && d.repeat_request)
        history = "Last three times you said " + countWord(d.requested) + ", it was " + countWord(d.effective) + ". ";
      else if (d.effective > d.requested)
        history = "When you say " + countWord(d.requested) + ", it usually turns into " + countWord(d.effective) + ". ";
      if (d.verdict == "GRANT") {
        auto line = say(*call, "snooze_grant", facts,
                        "Okay, " + minutesWord(d.requested) + ". " + history + "Even then you're on time. I'll call back.");
        line.end_call = true;
        stage_.erase(call_id);
        ctx_.wake_ladder->onSnooze(call_id, d.effective);  // call back when the real snooze runs out (T10)
        return line;
      }
      if (d.verdict == "COUNTER") {
        return say(*call, "snooze_counter", facts,
                   history + d.consequences.summary + " I'll give you " + minutesWord(d.counter) + ", not " +
                       countWord(d.requested) + ". Deal?");
      }
      return say(*call, "snooze_refuse", facts, "No. " + history + d.consequences.summary + " Feet on the floor, now.");
    }

    if (intent.label == "LEFT") {
      auto line = say(*call, "arrived", {}, "Love it. Go. I'll keep an eye on the timing.");
      line.end_call = true;
      stage_.erase(call_id);
      return line;
    }

    if (intent.label == "ASK_CONSEQUENCES") {
      int delay = intent.minutes > 0 ? intent.minutes : 10;
      auto report = DayPlanner(ctx_).consequences(call->user_id, delay);
      return say(*call, "awake_check", {{"minutes", delay}, {"consequences", report}}, report.summary);
    }

    bool checking = stage_[call_id] == "awake_check";
    bool real_answer = intent.label != "STALLING" && std::count(text.begin(), text.end(), ' ') >= 1;
    if (checking && real_answer) {
      // Passed the awake check: a real answer, not a grunt.
      ctx_.wake_ladder->onAwake(call_id);
      auto line = say(*call, "awake_verified", {}, "Good. You're up. I'll check back in five minutes.");
      line.end_call = true;
      stage_.erase(call_id);
      return line;
    }
    if (intent.label == "AWAKE" || intent.label == "READY" || checking) {
      stage_[call_id] = "awake_check";
      return say(*call, "awake_check", {}, checking ? "Not convinced. Tell me, what's your first thing today?"
                                                    : "Prove it. What's your first thing today?");
    }
    // STALLING or UNCLEAR before the check: keep pressing with the numbers.
    stage_[call_id] = "awake_check";
    auto report = DayPlanner(ctx_).consequences(call->user_id, 10);
    return say(*call, "awake_check", {{"minutes", 10}, {"consequences", report}},
               report.summary + " Come on. What's your first thing today?");
  }

  if (event == "declined" || event == "no_answer") {
    call->state = "no_answer";
    ctx_.updateCall(*call);
    stage_.erase(call_id);
    ctx_.wake_ladder->onNoAnswer(call_id);
    return std::nullopt;
  }

  if (event == "completed") {
    call->state = "completed";
    ctx_.updateCall(*call);
    stage_.erase(call_id);
  }
  return std::nullopt;
}

}  // namespace enro
