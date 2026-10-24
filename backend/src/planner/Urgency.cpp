#include "planner/Urgency.hpp"

namespace enro {

UrgencyWeights UrgencyWeights::fromConfig(const nlohmann::json& cfg) {
  UrgencyWeights w;
  if (cfg.contains("urgency")) {
    auto& u = cfg["urgency"];
    if (u.contains("weights")) {
      auto& ws = u["weights"];
      w.event_type = ws.value("event_type", w.event_type);
      w.slack = ws.value("slack", w.slack);
      w.load = ws.value("load", w.load);
      w.deadline = ws.value("deadline", w.deadline);
      w.bias = ws.value("bias", w.bias);
    }
    if (u.contains("levels")) {
      w.firm_at = u["levels"].value("FIRM", w.firm_at);
      w.urgent_at = u["levels"].value("URGENT", w.urgent_at);
    }
  }
  if (cfg.contains("types")) {
    for (auto& [type, t] : cfg["types"].items()) w.type_weight[type] = t.value("urgency_weight", 0.5);
  }
  return w;
}

UrgencyResult UrgencyScorer::score(const UrgencyInput& in) const {
  // STUB
  // TODO(T06): weighted sum of event type weight, slack, events today, deadline, and learned bias per PRD 7.3;
  // map to CHILL/FIRM/URGENT with w_.firm_at / w_.urgent_at; one reasoning line per input.
  return {50, Urgency::FIRM, {"urgency FIRM (50) [stub] for " + in.event.title}};
}

}  // namespace enro
