// Urgency score 0..100 and level CHILL | FIRM | URGENT (PRD 7.3). Weights live in one config
// struct loaded from backend/config/event_types.json. Owner: T06.
#pragma once

#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "models/Models.hpp"

namespace enro {

struct UrgencyWeights {
  double event_type = 0.4, slack = 0.3, load = 0.1, deadline = 0.1, bias = 0.1;
  int firm_at = 40, urgent_at = 70;            // score thresholds
  std::map<std::string, double> type_weight;  // event_type -> 0..1 (interview high, casual low)

  // Reads {"urgency": {...}, "types": {"<type>": {"urgency_weight": x}}} from event_types.json.
  static UrgencyWeights fromConfig(const nlohmann::json& event_types);
};

struct UrgencyInput {
  Event event;
  int slack_minutes = 0;   // minutes between leave_by and now (or wake), negative = already late
  int events_today = 1;
  bool has_deadline = false;
  PunctualityStats bias;   // learned stats for this event type
};

struct UrgencyResult {
  int score = 0;
  Urgency level = Urgency::CHILL;
  std::vector<std::string> reasoning;
};

class UrgencyScorer {
 public:
  explicit UrgencyScorer(UrgencyWeights weights) : w_(std::move(weights)) {}
  UrgencyResult score(const UrgencyInput& in) const;
  const UrgencyWeights& weights() const { return w_; }

 private:
  UrgencyWeights w_;
};

}  // namespace enro
