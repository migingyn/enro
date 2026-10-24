#include "reflex/JevReflex.hpp"

#include "util/Log.hpp"

namespace enro {

JevReflex::JevReflex(const Env& env)
    : api_key_(env.get("TYPESAFE_API_KEY")),
      model_(env.get("JEV_MODEL")),
      openrouter_key_(env.get("OPENROUTER_API_KEY")),
      timeout_ms_(env.getInt("JEV_TIMEOUT_MS", 400)) {
  if (api_key_.empty() && openrouter_key_.empty()) LOG_WARN("jev", "REFLEX_PROVIDER=jev but no key, using rules");
}

std::optional<ReflexVerdict> JevReflex::ask(const std::string&, const nlohmann::json&, const nlohmann::json&) {
  // STUB
  // TODO(T14): POST to Jev with cpr (timeout_ms_), parse the verdict, measure latency_ms, source "jev".
  return std::nullopt;
}

// Each decision: try Jev, fall back to rules. Rules fallback keeps source "rules".
#define ENRO_JEV_DECISION(method, decision, ...)                                    \
  ReflexVerdict JevReflex::method(const nlohmann::json& input) {                    \
    if (auto v = ask(decision, nlohmann::json::array({__VA_ARGS__}), input)) return *v; \
    return fallback_.method(input);                                                 \
  }

ENRO_JEV_DECISION(emailRelevance, "email_relevance", "IGNORE", "MAYBE", "PLAN_CHANGE")
ENRO_JEV_DECISION(transitSpeak, "transit_speak", "IGNORE", "COMMENT", "ALERT", "REROUTE")
ENRO_JEV_DECISION(interruptChannel, "interrupt_channel", "WAIT", "EARBUD", "CALL")
ENRO_JEV_DECISION(readiness, "readiness", "READY", "STALLING", "UNCLEAR")
ENRO_JEV_DECISION(snooze, "snooze", "GRANT", "COUNTER", "REFUSE")
ENRO_JEV_DECISION(stopCondition, "stop_condition", "CONTINUE", "BACK_OFF", "STOP", "ESCALATE")

#undef ENRO_JEV_DECISION

}  // namespace enro
