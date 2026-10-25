// Travel time interface. GoogleRouter (Routes API) when GOOGLE_MAPS_API_KEY is set,
// FallbackRouter (fixed speeds per mode) otherwise or on any error.
#pragma once

#include <map>
#include <optional>
#include <string>

#include "config/Env.hpp"
#include "google/GoogleHttp.hpp"

namespace enro {

struct Place {
  std::string label;  // address or name, e.g. Event.location
  std::optional<double> lat, lng;
};

struct RouteEta {
  int minutes = 0;
  int distance_m = 0;
  std::string mode;
  std::string source;  // "google" | "fallback"
  // Bus only, when the router knows the real walks (Google): minutes on foot before the first ride and
  // after it (incl. transfers). -1 = unknown, so the planner uses mode_legs from event_types.json.
  int walk_to_minutes = -1, walk_from_minutes = -1;
};

class Router {
 public:
  virtual ~Router() = default;
  virtual std::string name() const = 0;
  // mode: walk | bike | bus | drive
  virtual RouteEta eta(const std::string& mode, const Place& from, const Place& to) = 0;
};

class FallbackRouter : public Router {
 public:
  // speeds_kmh overrides the defaults (walk 4.8, bike 15, bus 18, drive 35).
  explicit FallbackRouter(std::map<std::string, double> speeds_kmh = {});
  std::string name() const override { return "fallback"; }
  RouteEta eta(const std::string& mode, const Place& from, const Place& to) override;

  // Straight-line distance, or default_distance_m when either side has no coordinates.
  static int distanceMeters(const Place& from, const Place& to, int default_distance_m = 1500);

 private:
  std::map<std::string, double> speeds_;
};

// Google Maps Routes API. Owner: T03 (Google integrations; routing is P1 in PRD 6).
// Falls back to `fallback` on any HTTP or parse error.
class GoogleRouter : public Router {
 public:
  // http defaults to cpr; tests inject a fake.
  GoogleRouter(const Env& env, Router& fallback, HttpClient http = {});
  std::string name() const override { return "google"; }
  // For bus, the ride only: ReadyToLeave adds the walks to and from the stop itself (mode_legs).
  RouteEta eta(const std::string& mode, const Place& from, const Place& to) override;
  // Door to door, for a known arrival deadline (epoch s). Lets drive refine by departure time and bus work
  // from the first scheduled departure instead of arrival - duration.
  RouteEta etaArriving(const std::string& mode, const Place& from, const Place& to, long long arrive_by);

  // A Routes estimate older than this is stale and should be asked for again.
  static constexpr long long kStaleSeconds = 15 * 60;

  struct Leg {
    long long seconds = 0;
    int distance_m = 0;
    long long first_transit_departure = 0;  // epoch s, transit only
    long long walk_before_transit_s = 0;
    long long walk_total_s = 0;  // every non-transit step (to, between and from stops), transit only
  };
  // routes[0] of a computeRoutes response, or nullopt if there is no usable route.
  static std::optional<Leg> parseRoute(const std::string& body);
  // "123s" / "123.4s" -> seconds (fractions round up), or -1.
  static long long parseDuration(const std::string& s);

 private:
  std::optional<Leg> request(const std::string& mode, const Place& from, const Place& to, long long departure_ts,
                             long long arrival_ts);

  std::string api_key_;
  Router& fallback_;
  HttpClient http_;
};

}  // namespace enro
