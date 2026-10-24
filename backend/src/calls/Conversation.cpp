#include "calls/Conversation.hpp"

#include <algorithm>

#include "app/App.hpp"
#include "util/Time.hpp"

namespace enro {

AgentLine Conversation::say(const Call& call, const std::string& purpose, const nlohmann::json& facts) {
  auto user = ctx_.repos->user(call.user_id);
  LineRequest req;
  req.purpose = purpose;
  req.persona = user ? user->archetype_id : "best_friend";
  req.urgency = call.attempt >= 3 ? Urgency::URGENT : call.attempt == 2 ? Urgency::FIRM : Urgency::CHILL;
  req.user_name = user ? user->name : "";
  req.facts = facts;
  AgentLine line{ctx_.lines->generate(req), req.persona, req.urgency, false};
  ctx_.recordTurn({call.id, "agent", line.text, "", time::nowTs()});
  return line;
}

std::optional<AgentLine> Conversation::onEvent(const std::string& call_id, const std::string& event,
                                               const std::string& text) {
  auto call = ctx_.repos->call(call_id);
  if (!call) return std::nullopt;

  if (event == "answered") {
    call->state = "in_progress";
    ctx_.updateCall(*call);
    // STUB
    // TODO(T11): opener with consequences ("If you sleep 10 more minutes you miss the bus"), then the awake check.
    std::string purpose = call->purpose == "wake" ? "wake_call_" + std::to_string(std::min(call->attempt, 3)) : call->purpose;
    return say(*call, purpose, {});
  }

  if (event == "speech") {
    Intent intent = ctx_.intents->classify(text, {{"purpose", call->purpose}, {"attempt", call->attempt}});
    ctx_.recordTurn({call_id, "user", text, intent.label, time::nowTs()});
    call->turns += 1;
    ctx_.updateCall(*call);
    // STUB
    // TODO(T11): branch on intent: SNOOZE_REQUEST -> ctx_.consequences->evaluate(delay = minutes * snooze_inflation)
    // + ctx_.reflex->snooze -> snooze_grant/counter/refuse with the numbers ("Last three times you said five,
    // it was eleven"); AWAKE -> awake check -> ctx_.wake_ladder->onAwake; READY/LEFT -> end the call.
    return say(*call, "awake_check", {});
  }

  if (event == "declined" || event == "no_answer") {
    call->state = "no_answer";
    ctx_.updateCall(*call);
    ctx_.wake_ladder->onNoAnswer(call_id);
    return std::nullopt;
  }

  if (event == "completed") {
    call->state = "completed";
    ctx_.updateCall(*call);
  }
  return std::nullopt;
}

}  // namespace enro
