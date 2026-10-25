#include "transit/Commentator.hpp"

#include <algorithm>
#include <cmath>

#include "models/Json.hpp"

namespace enro {

namespace {

constexpr double WALK_M_PER_MIN = 80;  // about 5 km/h

double meters(double lat1, double lng1, double lat2, double lng2) {
  constexpr double R = 6371000, RAD = 3.14159265358979323846 / 180;
  double dlat = (lat2 - lat1) * RAD, dlng = (lng2 - lng1) * RAD;
  double a = std::sin(dlat / 2) * std::sin(dlat / 2) +
             std::cos(lat1 * RAD) * std::cos(lat2 * RAD) * std::sin(dlng / 2) * std::sin(dlng / 2);
  return 2 * R * std::asin(std::sqrt(a));
}

double segment(const PlannedStop& a, const PlannedStop& b) { return meters(a.lat, a.lng, b.lat, b.lng); }

}  // namespace

Commentator::Commentator(Reflex& reflex, LineGenerator& lines) : reflex_(reflex), lines_(lines) {}

void Commentator::reset() {
  trip_id_.clear();
  last_stop_.clear();
  rerouted_ = false;
}

long long Commentator::estimateEta(const TripInfo& trip, const VehiclePosition& pos) {
  const auto& stops = trip.stops;
  long long planned_s = trip.scheduled_arrival_ts - trip.start_ts;
  if (stops.size() < 2 || trip.start_ts <= 0 || planned_s <= 0) return trip.scheduled_arrival_ts;

  double total = 0;
  for (size_t i = 1; i < stops.size(); ++i) total += segment(stops[i - 1], stops[i]);
  if (total <= 0) return trip.scheduled_arrival_ts;
  double pace = total / double(planned_s);  // meters per second

  // Distance to the next stop, then the rest of the planned route from there.
  size_t next = stops.size() - 1;
  for (size_t i = 1; i < stops.size(); ++i)
    if (stops[i].name == pos.next_stop) { next = i; break; }
  double remaining = meters(pos.lat, pos.lng, stops[next].lat, stops[next].lng);
  for (size_t i = next + 1; i < stops.size(); ++i) remaining += segment(stops[i - 1], stops[i]);
  return pos.ts + static_cast<long long>(std::llround(remaining / pace));
}

TripTick Commentator::tick(const TripInfo& trip, const VehiclePosition& pos) {
  if (trip.trip_id != trip_id_) {
    reset();
    trip_id_ = trip.trip_id;
  }

  TripTick t;
  t.trip_id = trip.trip_id;
  t.seq = pos.seq;
  t.ts = pos.ts;
  t.lat = pos.lat;
  t.lng = pos.lng;
  t.route = trip.route;
  t.next_stop = pos.next_stop;
  t.eta_ts = estimateEta(trip, pos);
  t.scheduled_arrival_ts = trip.scheduled_arrival_ts;
  t.delay_minutes = static_cast<int>(std::llround((t.eta_ts - t.scheduled_arrival_ts) / 60.0));
  t.on_route = pos.on_route;

  nlohmann::json input = t;
  input["stop_changed"] = !last_stop_.empty() && pos.next_stop != last_stop_;
  last_stop_ = pos.next_stop;
  ReflexVerdict v = reflex_.transitSpeak(input);
  if (v.verdict == "REROUTE") {
    if (rerouted_) v.verdict = "IGNORE";  // still on the same detour, already told them
    rerouted_ = true;
  }
  if (pos.on_route) rerouted_ = false;  // back on route: a later detour gets its own reroute
  t.verdict = v.verdict;
  last_verdict_ = v;
  last_inputs_ = input;

  long long minutes_left = std::max(0LL, (t.eta_ts - pos.ts) / 60);
  LineRequest req;
  req.persona = "best_friend";
  req.urgency = v.verdict == "IGNORE" || v.verdict == "COMMENT" ? Urgency::CHILL : Urgency::FIRM;
  req.facts = {{"event", trip.destination}, {"next_stop", pos.next_stop}, {"delay_minutes", t.delay_minutes},
               {"minutes", minutes_left}};

  if (v.verdict == "REROUTE") {
    // Walk from the next stop if that beats riding out the detour, otherwise stay on.
    const PlannedStop* dest = trip.stops.empty() ? nullptr : &trip.stops.back();
    long long walk_min = dest ? std::llround(meters(pos.lat, pos.lng, dest->lat, dest->lng) / WALK_M_PER_MIN) : 0;
    long long walk_eta = pos.ts + walk_min * 60;
    Reroute r;
    r.reason = "Vehicle left planned route near " + pos.next_stop;
    if (dest && walk_eta < t.eta_ts) {
      r.suggestion = "Get off at the next stop and walk " + std::to_string(walk_min) + " minutes";
      r.new_eta_ts = walk_eta;
    } else {
      r.suggestion = "Stay on: the detour rejoins the route at " + pos.next_stop + ", about " +
                     std::to_string(std::max(0, t.delay_minutes)) + " minutes late";
      r.new_eta_ts = t.eta_ts;
    }
    req.facts["suggestion"] = r.suggestion;
    t.reroute = r;
  }

  if (v.verdict == "COMMENT") req.purpose = "transit_comment";
  if (v.verdict == "ALERT") {
    req.purpose = "transit_alert";
    req.facts["minutes"] = t.delay_minutes;
  }
  if (v.verdict == "REROUTE") req.purpose = "transit_reroute";
  if (!req.purpose.empty()) t.line = lines_.generate(req);
  return t;
}

}  // namespace enro
