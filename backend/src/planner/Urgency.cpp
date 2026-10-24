#include "planner/Urgency.hpp"

#include <algorithm>
#include <cmath>

#include "models/Json.hpp"

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
  // Each input is normalized to 0..1, weighted, and summed. Weights sum to 1, so the score is 0..100.
  auto clamp01 = [](double x) { return std::clamp(x, 0.0, 1.0); };
  const std::string& type = in.event.event_type;

  double type_w = w_.type_weight.count(type) ? w_.type_weight.at(type) : defaultTypeWeight(type);
  // Zero or negative slack is maximal pressure. Four hours or more of slack is none.
  double slack = in.slack_minutes <= 0 ? 1.0 : clamp01(1.0 - in.slack_minutes / 240.0);
  double load = clamp01((in.events_today - 1) / 4.0);
  double deadline = in.has_deadline ? 1.0 : 0.0;
  // Running late for this type before (prep bias or late arrivals) means push harder now.
  double bias = clamp01(std::max(in.bias.prep_bias, in.bias.arrival_delta) / 15.0);

  double sum = w_.event_type * type_w + w_.slack * slack + w_.load * load + w_.deadline * deadline + w_.bias * bias;
  double total = w_.event_type + w_.slack + w_.load + w_.deadline + w_.bias;
  int score = static_cast<int>(std::lround(100.0 * sum / (total > 0 ? total : 1.0)));
  score = std::clamp(score, 0, 100);

  UrgencyResult r;
  r.score = score;
  r.level = score >= w_.urgent_at ? Urgency::URGENT : score >= w_.firm_at ? Urgency::FIRM : Urgency::CHILL;
  auto pts = [](double weight, double term) { return std::to_string(static_cast<int>(std::lround(100 * weight * term))); };
  r.reasoning = {
      "event type " + (type.empty() ? std::string("unknown") : type) + " +" + pts(w_.event_type, type_w),
      "slack " + std::to_string(in.slack_minutes) + " min +" + pts(w_.slack, slack),
      std::to_string(in.events_today) + " events today +" + pts(w_.load, load),
      std::string(in.has_deadline ? "hard start" : "no hard start") + " +" + pts(w_.deadline, deadline),
      "learned bias +" + pts(w_.bias, bias),
      "urgency " + nlohmann::json(r.level).get<std::string>() + " (" + std::to_string(score) + ")",
  };
  return r;
}

double UrgencyScorer::defaultTypeWeight(const std::string& type) {
  // PRD 7.3: interview, exam, flight high; class, meeting medium; casual low.
  static const std::map<std::string, double> defaults = {{"interview", 0.95}, {"exam", 0.95}, {"flight", 1.0},
                                                         {"class", 0.5},      {"meeting", 0.5}, {"casual", 0.2}};
  auto it = defaults.find(type);
  return it == defaults.end() ? 0.5 : it->second;
}

}  // namespace enro
