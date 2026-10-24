#include "calls/WakeLadder.hpp"

#include "app/App.hpp"
#include "util/Log.hpp"
#include "util/Time.hpp"

namespace enro {

std::string WakeLadder::callNow(const std::string& user_id, const std::string& purpose, int attempt) {
  auto user = ctx_.repos->user(user_id);
  Call call;
  call.id = "call_" + std::to_string(time::nowMs());
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

std::string WakeLadder::start(const std::string& user_id) {
  // STUB
  // TODO(T10): push WAKE_CALL tasks for CALL_1/2/3 at alarm+grace, +3, +3 (ctx_.queue), with ctx_.reflex->stopCondition
  // deciding each step; on CALL_3 also ctx_.contacts->notifyAll. For now: ring CALL_1 immediately.
  LOG_INFO("ladder", "wake ladder for " << user_id << " (stub: CALL_1 only)");
  return callNow(user_id, "wake", 1);
}

WakePlan WakeLadder::plan(const User& user, const std::string& date) const {
  // STUB
  // TODO(T10): steps from alarm_time + grace_minutes, then +3 min each, CALL_3 with notify_contacts=true,
  // AWAKE_VERIFY 5 min after CALL_3 (see contracts/examples/plan.json "wake").
  WakePlan w;
  w.alarm_time = user.alarm_time;
  w.alarm_ts = time::atLocalTime(date, user.alarm_time.empty() ? "07:00" : user.alarm_time, user.timezone);
  w.grace_minutes = user.grace_minutes;
  return w;
}

void WakeLadder::onNoAnswer(const std::string& call_id) {
  // STUB
  // TODO(T10): mark the call no_answer and let the next scheduled step fire (or escalate now in demo mode).
  LOG_INFO("ladder", call_id << " not answered (stub: no escalation yet)");
}

void WakeLadder::onAwake(const std::string& call_id) {
  // STUB
  // TODO(T10): cancel remaining ladder tasks, schedule the AWAKE_VERIFY call 5 minutes out.
  LOG_INFO("ladder", call_id << " awake (stub: no verify call yet)");
}

}  // namespace enro
