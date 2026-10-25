// Departure verification (PRD 5 daily use step 4, P1). The Beacon page posts the phone's location;
// a good fix more than kDepartedRadiusM from home marks the user as out the door for the day, and
// DEPARTURE_CHECK nudge calls stop. Owner: T10.
// Real trips also teach the punctuality model (T13): departure starts a trip toward the next planned event,
// and the first good fix within kArrivedRadiusM of its location ends it as an Outcome (walk or bus,
// walking legs included), so travel factors come from real movement, not just seed history.
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
  static constexpr double kArrivedRadiusM = 150;

  explicit Departure(AppContext& ctx) : ctx_(ctx) {}

  LocationResult onLocation(const std::string& user_id, double lat, double lng, double accuracy_m, long long ts);
  bool departedToday(const std::string& user_id) const;

  // DEPARTURE_CHECK: ring a departure_check call unless the user already left today.
  void registerHandlers(Scheduler& scheduler);
  void handleCheck(const AgentTask& task);

 private:
  std::string key(const std::string& user_id) const;  // kv "departed:<user>:<date>"
  void startTrip(const std::string& user_id, long long ts);
  // Ends the active trip if (lat, lng) is at its destination. Returns true when an Outcome was recorded.
  bool maybeArrive(const std::string& user_id, double lat, double lng, long long ts);
  AppContext& ctx_;
};

}  // namespace enro
