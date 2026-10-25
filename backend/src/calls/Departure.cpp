#include "calls/Departure.hpp"

#include "app/App.hpp"
#include "models/Json.hpp"
#include "planner/DayPlanner.hpp"
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
  if (accuracy_m <= kMaxAccuracyM) maybeArrive(user_id, lat, lng, ts);
  if (!home.lat || accuracy_m > kMaxAccuracyM || meters <= kDepartedRadiusM) return r;

  r.departed = true;
  if (!departedToday(user_id)) {
    ctx_.repos->setKv(key(user_id), std::to_string(ts));
    // The user is out the door: equivalent to a READY reply on a leave call.
    ctx_.recordDecision(user_id, "readiness", {"READY", 0.9, "rules", 0},
                        {{"source", "location"}, {"distance_from_home_m", static_cast<int>(meters)}});
    LOG_INFO("departure", user_id << " left home (" << static_cast<int>(meters) << " m)");
    startTrip(user_id, ts);
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

namespace {
std::string tripKey(const std::string& user_id) { return "trip:" + user_id; }
}  // namespace

void Departure::startTrip(const std::string& user_id, long long ts) {
  auto user = ctx_.repos->user(user_id);
  if (!user) return;
  DayPlan day = DayPlanner(ctx_).build(user_id);
  const LeavePlan* next = nullptr;
  const Event* event = nullptr;
  for (auto& p : day.plans) {
    for (auto& e : day.events) {
      if (e.id == p.event_id && !e.canceled && e.start_ts > ts && (!event || e.start_ts < event->start_ts)) {
        next = &p;
        event = &e;
      }
    }
  }
  if (!next) return;
  Place dest = ctx_.places->resolve(event->location);
  if (!dest.lat) return;  // can't detect arrival without coordinates

  // Raw (unfactored) prediction, so the model learns actual / predicted: map ETA plus walking legs.
  int predicted = ctx_.router->eta(next->mode, ctx_.places->resolve(user->home_area), dest).minutes;
  if (ctx_.event_types.contains("mode_legs") && ctx_.event_types["mode_legs"].contains(next->mode)) {
    auto& legs = ctx_.event_types["mode_legs"][next->mode];
    predicted += legs.value("walk_to_stop_minutes", 0) + legs.value("walk_from_stop_minutes", 0);
  }
  nlohmann::json trip = {{"event_id", event->id}, {"event_type", event->event_type}, {"mode", next->mode},
                         {"planned_leave", next->leave_by}, {"actual_leave", ts}, {"start_ts", event->start_ts},
                         {"predicted_travel_minutes", predicted}, {"dest_lat", *dest.lat}, {"dest_lng", *dest.lng}};
  ctx_.repos->setKv(tripKey(user_id), trip.dump());
  LOG_INFO("departure", user_id << " heading to " << event->title << " by " << next->mode << " (predicted " << predicted
                                << " min)");
}

bool Departure::maybeArrive(const std::string& user_id, double lat, double lng, long long ts) {
  auto raw = ctx_.repos->kv(tripKey(user_id));
  if (!raw || raw->empty()) return false;
  auto trip = nlohmann::json::parse(*raw);
  Place dest{"destination", trip["dest_lat"].get<double>(), trip["dest_lng"].get<double>()};
  if (FallbackRouter::distanceMeters(dest, {"here", lat, lng}) > kArrivedRadiusM) return false;

  Outcome o{user_id, trip["event_id"], trip["event_type"], trip["mode"],
            trip["planned_leave"], trip["actual_leave"], ts};
  ctx_.repos->insertOutcome(o);
  ctx_.punctuality->addOutcome(o, trip["predicted_travel_minutes"].get<int>(), trip["start_ts"].get<long long>());
  ctx_.repos->setKv(tripKey(user_id), "");  // one Outcome per trip
  LOG_INFO("departure", user_id << " arrived at " << o.event_id << " by " << o.mode << " in "
                                << (o.arrival - o.actual_leave) / 60 << " min (predicted "
                                << trip["predicted_travel_minutes"].get<int>() << ")");
  // The learned factors moved: show the updated plan.
  ctx_.dashboard->broadcast("plan", DayPlanner(ctx_).build(user_id));
  return true;
}

}  // namespace enro
