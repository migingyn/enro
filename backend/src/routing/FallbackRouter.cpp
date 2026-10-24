#include <algorithm>
#include <cmath>
#include <numbers>

#include "routing/Router.hpp"

namespace enro {

FallbackRouter::FallbackRouter(std::map<std::string, double> speeds_kmh)
    : speeds_{{"walk", 4.8}, {"bike", 15.0}, {"bus", 18.0}, {"drive", 35.0}} {
  for (auto& [mode, kmh] : speeds_kmh) speeds_[mode] = kmh;
}

int FallbackRouter::distanceMeters(const Place& from, const Place& to, int default_distance_m) {
  if (!from.lat || !from.lng || !to.lat || !to.lng) return default_distance_m;
  constexpr double kEarthM = 6371000.0;
  constexpr double kRad = std::numbers::pi / 180.0;
  double dlat = (*to.lat - *from.lat) * kRad;
  double dlng = (*to.lng - *from.lng) * kRad;
  double a = std::sin(dlat / 2) * std::sin(dlat / 2) +
             std::cos(*from.lat * kRad) * std::cos(*to.lat * kRad) * std::sin(dlng / 2) * std::sin(dlng / 2);
  return static_cast<int>(kEarthM * 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a)));
}

RouteEta FallbackRouter::eta(const std::string& mode, const Place& from, const Place& to) {
  double kmh = speeds_.count(mode) ? speeds_[mode] : speeds_["walk"];
  int meters = distanceMeters(from, to);
  int minutes = std::max(1, static_cast<int>(std::ceil(meters / 1000.0 / kmh * 60.0)));
  return {minutes, meters, mode, "fallback"};
}

}  // namespace enro
