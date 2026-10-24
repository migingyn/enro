// Reflex layer (PRD 7.11): fast typed decisions about WHETHER to act. The LLM still writes
// every spoken line. One method per decision. Selected by REFLEX_PROVIDER: rules | jev.
// Inputs are JSON because they are stored verbatim in Decision.inputs for the dashboard.
#pragma once

#include <string>

#include <nlohmann/json.hpp>

namespace enro {

struct ReflexVerdict {
  std::string verdict;
  double confidence = 1.0;
  std::string source = "rules";  // "jev" | "rules"
  int latency_ms = 0;
};

class Reflex {
 public:
  virtual ~Reflex() = default;
  virtual std::string name() const = 0;

  // IGNORE | MAYBE | PLAN_CHANGE. Input: {message_id, from, subject, snippet}
  virtual ReflexVerdict emailRelevance(const nlohmann::json& email) = 0;
  // IGNORE | COMMENT | ALERT | REROUTE. Input: a TripTick-like object
  virtual ReflexVerdict transitSpeak(const nlohmann::json& tick) = 0;
  // WAIT | EARBUD | CALL. Input: {urgency, minutes_to_leave, in_transit, time_of_day}
  virtual ReflexVerdict interruptChannel(const nlohmann::json& context) = 0;
  // READY | STALLING | UNCLEAR. Input: {text, intent}
  virtual ReflexVerdict readiness(const nlohmann::json& reply) = 0;
  // GRANT | COUNTER | REFUSE. Input: {requested_minutes, consequences, snooze_inflation}
  virtual ReflexVerdict snooze(const nlohmann::json& request) = 0;
  // CONTINUE | BACK_OFF | STOP | ESCALATE. Input: {attempt, answered, awake, minutes_since_alarm}
  virtual ReflexVerdict stopCondition(const nlohmann::json& state) = 0;
};

}  // namespace enro
