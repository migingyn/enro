#include "reflex/RulesReflex.hpp"

namespace enro {

// Every method returns a fixed verdict so the skeleton runs end to end.
// T14 owns these rules (they are also the fallback when Jev times out).

ReflexVerdict RulesReflex::emailRelevance(const nlohmann::json&) {
  // STUB
  // TODO(T14): rules on subject/sender/snippet (canceled, moved, rescheduled, room change) per PRD 7.2.
  return {"MAYBE", 0.5};
}

ReflexVerdict RulesReflex::transitSpeak(const nlohmann::json&) {
  // STUB
  // TODO(T14): REROUTE when off route, ALERT when delay grows past slack, COMMENT every few stops.
  return {"COMMENT", 0.5};
}

ReflexVerdict RulesReflex::interruptChannel(const nlohmann::json&) {
  // STUB
  // TODO(T14): CALL for URGENT or morning, EARBUD in transit, WAIT when nothing is due.
  return {"CALL", 0.5};
}

ReflexVerdict RulesReflex::readiness(const nlohmann::json&) {
  // STUB
  // TODO(T14): READY / STALLING from the classified intent.
  return {"UNCLEAR", 0.5};
}

ReflexVerdict RulesReflex::snooze(const nlohmann::json&) {
  // STUB
  // TODO(T14): GRANT if consequences stay OK after snooze_inflation, COUNTER if AT_RISK, REFUSE if MISSED.
  return {"COUNTER", 0.5};
}

ReflexVerdict RulesReflex::stopCondition(const nlohmann::json&) {
  // STUB
  // TODO(T14): ESCALATE after CALL_3 unanswered, STOP once awake verified, BACK_OFF otherwise.
  return {"CONTINUE", 0.5};
}

}  // namespace enro
