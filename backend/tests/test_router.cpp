// routing/FallbackRouter (fixed speeds). Shared; GoogleRouter is T03.
#include <doctest/doctest.h>

#include "routing/Router.hpp"

using namespace enro;

TEST_CASE("fallback router uses fixed speeds per mode") {
  FallbackRouter r;
  Place a{"home", 32.8801, -117.2340}, b{"Center Hall", 32.8776, -117.2373};
  auto walk = r.eta("walk", a, b), bike = r.eta("bike", a, b);
  CHECK(walk.source == "fallback");
  CHECK(walk.minutes > bike.minutes);
  CHECK(r.eta("walk", {"x"}, {"y"}).distance_m == 1500);  // no coordinates -> default distance
}
