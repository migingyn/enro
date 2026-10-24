// Travel time interface. GoogleRouter (Routes API) when GOOGLE_MAPS_API_KEY is set,
// FallbackRouter (fixed speeds per mode) otherwise or on any error.
#pragma once

#include <map>
#include <optional>
#include <string>

#include "config/Env.hpp"

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
class GoogleRouter : public Router {
 public:
  GoogleRouter(const Env& env, Router& fallback);
  std::string name() const override { return "google"; }
  RouteEta eta(const std::string& mode, const Place& from, const Place& to) override;

 private:
  std::string api_key_;
  Router& fallback_;
};

}  // namespace enro
