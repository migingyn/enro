// Personal punctuality model (PRD 7.8). Per user, per event type, and per mode, exponentially
// weighted averages of prep_bias, travel_factor, arrival_delta, snooze_inflation; plus channel
// answer rates. Seeded from backend/seed/demo_history.json. Owner: T13.
#pragma once

#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "models/Models.hpp"

namespace enro {

class PunctualityModel {
 public:
  // Feed outcomes oldest first. predicted_travel_minutes is optional context from the seed history.
  void addOutcome(const Outcome& o, int predicted_travel_minutes = 0);
  void addSnooze(const std::string& event_type, int requested_minutes, int actual_minutes);
  void addChannelEvent(const std::string& channel, long long ts, bool answered);
  // Convenience: load the whole demo_history.json document.
  void loadHistory(const nlohmann::json& history);

  PunctualityStats forEventType(const std::string& event_type) const;
  PunctualityStats forMode(const std::string& mode) const;
  // Everything the dashboard's PunctualityCard needs, including human headlines.
  PunctualityReport report(const std::string& user_id) const;
};

}  // namespace enro
