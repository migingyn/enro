#include "routing/Router.hpp"
#include "util/Log.hpp"

namespace enro {

GoogleRouter::GoogleRouter(const Env& env, Router& fallback)
    : api_key_(env.get("GOOGLE_MAPS_API_KEY")), fallback_(fallback) {}

RouteEta GoogleRouter::eta(const std::string& mode, const Place& from, const Place& to) {
  // STUB
  // TODO(T03): POST https://routes.googleapis.com/directions/v2:computeRoutes (X-Goog-Api-Key, X-Goog-FieldMask:
  // routes.duration,routes.distanceMeters), travelMode WALK|BICYCLE|TRANSIT|DRIVE. Cache by (mode, from, to)
  // with LruCache (T05). Fall back on any error.
  return fallback_.eta(mode, from, to);
}

}  // namespace enro
