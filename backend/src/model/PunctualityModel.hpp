// Personal punctuality model (PRD 7.8). Per user, per event type, and per mode, exponentially
// weighted averages of prep_bias, travel_factor, arrival_delta, snooze_inflation; plus channel
// answer rates. Seeded from backend/seed/demo_history.json. Owner: T13.
#pragma once

#include <deque>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "model/TransportLearner.hpp"
#include "models/Models.hpp"
#include "util/Ewma.hpp"

namespace enro {

struct SnoozeSample {
  int requested_minutes = 0, actual_minutes = 0;
};

class PunctualityModel {
 public:
  // transport, when given, also learns modes from outcomes in loadHistory and feeds report().transport.
  explicit PunctualityModel(TransportLearner* transport = nullptr) : transport_(transport) {}

  // Feed outcomes oldest first. predicted_travel_minutes is optional context from the seed history.
  // event_start, when known, gives arrival_delta.
  void addOutcome(const Outcome& o, int predicted_travel_minutes = 0, long long event_start = 0);
  void addSnooze(const std::string& event_type, int requested_minutes, int actual_minutes);
  void addChannelEvent(const std::string& channel, long long ts, bool answered);
  // Convenience: load the whole demo_history.json document.
  void loadHistory(const nlohmann::json& history);

  PunctualityStats forEventType(const std::string& event_type) const;
  PunctualityStats forMode(const std::string& mode) const;
  // Most recent snoozes for a type, newest last (up to 10).
  std::vector<SnoozeSample> recentSnoozes(const std::string& event_type) const;
  // Everything the dashboard's PunctualityCard needs, including human headlines.
  PunctualityReport report(const std::string& user_id) const;

  // "morning" before 12:00, "day" before 18:00, else "evening" (America/Los_Angeles).
  static std::string timeOfDay(long long ts);

 private:
  struct TypeStats {
    Ewma prep{0.15}, travel{0.15, 1.0}, arrival{0.15}, snooze{0.3, 1.0};
    std::deque<SnoozeSample> recent;
  };
  struct ModeStats {
    Ewma travel{0.15, 1.0};
  };
  struct Rate {
    int answered = 0, total = 0;
  };

  TransportLearner* transport_;
  std::map<std::string, TypeStats> types_;
  std::map<std::string, ModeStats> modes_;
  std::map<std::pair<std::string, std::string>, Rate> channels_;
};

}  // namespace enro
