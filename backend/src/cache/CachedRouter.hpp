// Router decorator: caches ETAs by mode + from + to so repeated plans cost no Routes API calls (PRD 4).
// Owner: T05. Wrap whichever router App selects: CachedRouter cached(*ctx.router);
#pragma once

#include <string>

#include "cache/LruCache.hpp"
#include "routing/Router.hpp"
#include "util/Log.hpp"

namespace enro {

class CachedRouter : public Router {
 public:
  explicit CachedRouter(Router& inner, size_t capacity = 4096) : inner_(inner), cache_(capacity) {}
  std::string name() const override { return inner_.name() + "+cache"; }

  RouteEta eta(const std::string& mode, const Place& from, const Place& to) override {
    std::string key = mode + '|' + placeKey(from) + '|' + placeKey(to);
    if (auto hit = cache_.get(key)) return *hit;
    RouteEta r = inner_.eta(mode, from, to);
    cache_.put(key, r);
    if (++lookups_ % 100 == 0) LOG_INFO("cache", "route cache hit rate " << cache_.hitRate());
    return r;
  }

  double hitRate() const { return cache_.hitRate(); }

 private:
  static std::string placeKey(const Place& p) {
    if (p.lat && p.lng) return std::to_string(*p.lat) + ',' + std::to_string(*p.lng);
    return p.label;
  }

  Router& inner_;
  LruCache<std::string, RouteEta> cache_;
  size_t lookups_ = 0;
};

}  // namespace enro
