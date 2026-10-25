// Departure verification (PRD 5 daily use step 4, P1). The Beacon page posts the phone's location;
// a good fix more than kDepartedRadiusM from home marks the user as out the door for the day, and
// DEPARTURE_CHECK nudge calls stop. Owner: T10.
#pragma once

#include <string>

namespace enro {

struct AppContext;
class Scheduler;
struct AgentTask;

struct LocationResult {
  bool ok = false;
  bool departed = false;
  double distance_from_home_m = 0;
};

class Departure {
 public:
  static constexpr double kDepartedRadiusM = 300;
  static constexpr double kMaxAccuracyM = 200;  // worse fixes are ignored

  explicit Departure(AppContext& ctx) : ctx_(ctx) {}

  LocationResult onLocation(const std::string& user_id, double lat, double lng, double accuracy_m, long long ts);
  bool departedToday(const std::string& user_id) const;

  // DEPARTURE_CHECK: ring a departure_check call unless the user already left today.
  void registerHandlers(Scheduler& scheduler);
  void handleCheck(const AgentTask& task);

 private:
  std::string key(const std::string& user_id) const;  // kv "departed:<user>:<date>"
  AppContext& ctx_;
};

}  // namespace enro
