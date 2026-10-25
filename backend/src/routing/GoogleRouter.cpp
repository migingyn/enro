// Google Maps Routes API (computeRoutes). Owner: T03.
// https://developers.google.com/maps/documentation/routes/reference/rest/v2/TopLevel/computeRoutes
//   drive: TRAFFIC_AWARE_OPTIMAL, refined by departure time (up to 3 requests), longest duration kept.
//   bus (TRANSIT): when an arrival time is known, leave = first scheduled transit departure minus the
//        walking before it. A bus can arrive well before the deadline, so arrival - duration would miss it.
//   walk / bike: plain duration.
// Any HTTP or parse error -> the fallback router (fixed speeds). Google never "invents" a duration.
#include <algorithm>
#include <cmath>
#include <regex>

#include <nlohmann/json.hpp>

#include "routing/Router.hpp"
#include "util/Log.hpp"
#include "util/Time.hpp"

namespace enro {

namespace {

constexpr const char* kComputeRoutes = "https://routes.googleapis.com/directions/v2:computeRoutes";

std::string travelMode(const std::string& mode) {
  if (mode == "drive") return "DRIVE";
  if (mode == "bike") return "BICYCLE";
  if (mode == "bus") return "TRANSIT";
  return "WALK";
}

nlohmann::json waypoint(const Place& p) {
  if (p.lat && p.lng) return {{"location", {{"latLng", {{"latitude", *p.lat}, {"longitude", *p.lng}}}}}};
  return {{"address", p.label}};
}

}  // namespace

GoogleRouter::GoogleRouter(const Env& env, Router& fallback, HttpClient http)
    : api_key_(env.get("GOOGLE_MAPS_API_KEY")), fallback_(fallback), http_(http ? std::move(http) : cprHttpClient()) {}

long long GoogleRouter::parseDuration(const std::string& s) {
  static const std::regex re(R"(^(\d+)(\.\d{1,9})?s$)");
  std::smatch m;
  if (!std::regex_match(s, m, re)) return -1;
  return std::stoll(m[1]) + (m[2].matched ? 1 : 0);  // round fractions up
}

std::optional<GoogleRouter::Leg> GoogleRouter::parseRoute(const std::string& body) {
  auto j = nlohmann::json::parse(body, nullptr, false);
  if (j.is_discarded() || !j.contains("routes") || !j["routes"].is_array() || j["routes"].empty()) return std::nullopt;
  auto& route = j["routes"][0];
  Leg leg;
  leg.seconds = parseDuration(route.value("duration", ""));
  if (leg.seconds < 0) return std::nullopt;
  leg.distance_m = route.value("distanceMeters", 0);
  // Transit: walk time until the first scheduled departure, and all walking on the route.
  bool seen_transit = false;
  for (auto& l : route.value("legs", nlohmann::json::array())) {
    for (auto& step : l.value("steps", nlohmann::json::array())) {
      if (step.value("travelMode", "") == "TRANSIT" || step.contains("transitDetails")) {
        if (seen_transit) continue;
        std::string dep = step.contains("transitDetails")
                              ? step["transitDetails"].value("stopDetails", nlohmann::json::object()).value("departureTime", "")
                              : "";
        long long dep_ts = parseRfc3339(dep);
        if (dep_ts < 0) return std::nullopt;  // a transit leg without a schedule can't be planned around
        leg.first_transit_departure = dep_ts;
        leg.walk_before_transit_s = leg.walk_total_s;
        seen_transit = true;
        continue;
      }
      leg.walk_total_s += std::max(0LL, parseDuration(step.value("staticDuration", "0s")));
    }
  }
  if (!seen_transit) leg.walk_total_s = 0;  // not a transit route: nothing to split out
  return leg;
}

std::optional<GoogleRouter::Leg> GoogleRouter::request(const std::string& mode, const Place& from, const Place& to,
                                                       long long departure_ts, long long arrival_ts) {
  nlohmann::json body = {{"origin", waypoint(from)},
                         {"destination", waypoint(to)},
                         {"travelMode", travelMode(mode)},
                         {"computeAlternativeRoutes", false}};
  std::string mask = "routes.duration,routes.distanceMeters";
  if (mode == "drive") {
    body["routingPreference"] = "TRAFFIC_AWARE_OPTIMAL";
    if (departure_ts > 0) body["departureTime"] = toRfc3339(departure_ts);
  } else if (mode == "bus") {
    if (arrival_ts > 0) body["arrivalTime"] = toRfc3339(arrival_ts);
    mask += ",routes.legs.steps.staticDuration,routes.legs.steps.travelMode,"
            "routes.legs.steps.transitDetails.stopDetails.departureTime";
  }
  HttpResponse r = http_({"POST",
                          kComputeRoutes,
                          {{"Content-Type", "application/json"}, {"X-Goog-Api-Key", api_key_}, {"X-Goog-FieldMask", mask}},
                          body.dump(),
                          20000});
  if (r.status != 200) {
    LOG_WARN("routing", "Routes API HTTP " << r.status << " for " << mode << " " << from.label << " -> " << to.label);
    return std::nullopt;
  }
  auto leg = parseRoute(r.body);
  if (!leg) LOG_WARN("routing", "Routes API returned no usable route for " << mode << " -> " << to.label);
  return leg;
}

RouteEta GoogleRouter::eta(const std::string& mode, const Place& from, const Place& to) {
  if (mode != "bus") return etaArriving(mode, from, to, 0);
  auto leg = request(mode, from, to, 0, 0);
  if (!leg) return fallback_.eta(mode, from, to);
  // Google's transit duration is door to door; the planner adds its own walks, so return the ride.
  long long ride = std::max(60LL, leg->seconds - leg->walk_total_s);
  RouteEta out{static_cast<int>((ride + 59) / 60), leg->distance_m, mode, "google"};
  if (leg->first_transit_departure > 0) {
    auto mins = [](long long s) { return static_cast<int>((s + 59) / 60); };
    out.walk_to_minutes = mins(leg->walk_before_transit_s);
    out.walk_from_minutes = mins(leg->walk_total_s - leg->walk_before_transit_s);
  }
  return out;
}

RouteEta GoogleRouter::etaArriving(const std::string& mode, const Place& from, const Place& to, long long arrive_by) {
  long long now = time::nowTs();
  auto fallback = [&] { return fallback_.eta(mode, from, to); };
  if (arrive_by > 0 && arrive_by <= now) arrive_by = 0;  // past deadline: just "how long from now"

  // drive departs at the deadline first; others ask Google for "leave now" unless transit has a deadline.
  auto leg = request(mode, from, to, mode == "drive" && arrive_by > 0 ? arrive_by : 0, arrive_by);
  if (!leg) return fallback();
  long long seconds = leg->seconds;

  if (mode == "drive" && arrive_by > 0) {
    // Traffic depends on when you leave, which depends on traffic. Refine twice, keep the longest.
    for (int i = 0; i < 2; ++i) {
      long long depart = arrive_by - seconds;
      if (depart <= now) break;
      auto refined = request(mode, from, to, depart, 0);
      if (!refined) break;
      long long diff = std::llabs(refined->seconds - seconds);
      seconds = std::max(seconds, refined->seconds);
      leg->distance_m = refined->distance_m;
      if (diff <= 60) break;
    }
  } else if (mode == "bus" && arrive_by > 0 && leg->first_transit_departure > 0) {
    // Leave in time to walk to the first scheduled departure; the trip "takes" deadline - leave time.
    long long leave = leg->first_transit_departure - leg->walk_before_transit_s;
    seconds = std::max(seconds, arrive_by - leave);
  }
  int minutes = std::max(1, static_cast<int>((seconds + 59) / 60));
  return {minutes, leg->distance_m, mode, "google"};
}

}  // namespace enro
