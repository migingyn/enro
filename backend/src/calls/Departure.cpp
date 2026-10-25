#include "calls/Departure.hpp"

#include "app/App.hpp"
#include "util/Log.hpp"
#include "util/Time.hpp"

namespace enro {

std::string Departure::key(const std::string& user_id) const {
  auto user = ctx_.repos->user(user_id);
  std::string tz = user && !user->timezone.empty() ? user->timezone : "America/Los_Angeles";
  return "departed:" + user_id + ":" + time::dateIn(time::nowTs(), tz);
}

bool Departure::departedToday(const std::string& user_id) const { return ctx_.repos->kv(key(user_id)).has_value(); }

LocationResult Departure::onLocation(const std::string& user_id, double lat, double lng, double accuracy_m, long long ts) {
  auto user = ctx_.repos->user(user_id);
  if (!user) return {};
  Place home = ctx_.places->resolve(user->home_area);
  Place here{"beacon", lat, lng};
  // Unknown home coordinates: distanceMeters returns the default, so treat it as "not sure" (0 m).
  double meters = home.lat ? FallbackRouter::distanceMeters(home, here) : 0;
  LocationResult r{true, false, meters};
  if (!home.lat || accuracy_m > kMaxAccuracyM || meters <= kDepartedRadiusM) return r;

  r.departed = true;
  if (!departedToday(user_id)) {
    ctx_.repos->setKv(key(user_id), std::to_string(ts));
    // The user is out the door: equivalent to a READY reply on a leave call.
    ctx_.recordDecision(user_id, "readiness", {"READY", 0.9, "rules", 0},
                        {{"source", "location"}, {"distance_from_home_m", static_cast<int>(meters)}});
    LOG_INFO("departure", user_id << " left home (" << static_cast<int>(meters) << " m)");
  }
  return r;
}

void Departure::registerHandlers(Scheduler& scheduler) {
  scheduler.on(TaskKind::DEPARTURE_CHECK, [this](const AgentTask& t) { handleCheck(t); });
}

void Departure::handleCheck(const AgentTask& task) {
  if (departedToday(task.user_id)) return;  // already left: no nudge
  ctx_.wake_ladder->callNow(task.user_id, "departure_check", 1);
}

}  // namespace enro
