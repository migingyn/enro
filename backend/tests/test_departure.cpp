// T10: departure verification from the Beacon page (POST /api/location, P1).
#include <doctest/doctest.h>

#include "TestApp.hpp"
#include "calls/Departure.hpp"
#include "config/Places.hpp"
#include "store/Seed.hpp"
#include "util/Time.hpp"

using namespace enro;

namespace {
std::unique_ptr<AppContext> seeded() {
  auto app = test::fakeApp();
  seedDemo(*app);
  return app;
}
constexpr double kHomeLat = 32.8706, kHomeLng = -117.2284;  // places.json "Nobel Dr"
}  // namespace

TEST_CASE("places resolve by case-insensitive substring") {
  Places p = Places::fromConfig({{"places", {{{"match", "Lusk Blvd"}, {"label", "Q"}, {"lat", 1.0}, {"lng", 2.0}}}}});
  auto hit = p.resolve("Qualcomm Building Q, 6455 LUSK BLVD, San Diego");
  REQUIRE(hit.lat);
  CHECK(*hit.lat == 1.0);
  CHECK_FALSE(p.resolve("Somewhere else").lat);
  CHECK(p.resolve("Somewhere else").label == "Somewhere else");
}

TEST_CASE("a fix near home is not a departure; one past 300 m is") {
  auto app = seeded();
  auto near = app->departure->onLocation("u_demo", kHomeLat + 0.0005, kHomeLng, 10, time::nowTs());
  CHECK(near.ok);
  CHECK_FALSE(near.departed);
  CHECK(near.distance_from_home_m < 100);
  auto far = app->departure->onLocation("u_demo", 32.8776, -117.2373, 10, time::nowTs());  // Center Hall
  CHECK(far.departed);
  CHECK(far.distance_from_home_m > 300);
  CHECK(app->departure->departedToday("u_demo"));
}

TEST_CASE("an inaccurate fix never counts as departed") {
  auto app = seeded();
  auto r = app->departure->onLocation("u_demo", 32.8776, -117.2373, 900, time::nowTs());
  CHECK_FALSE(r.departed);
  CHECK_FALSE(app->departure->departedToday("u_demo"));
}

TEST_CASE("DEPARTURE_CHECK calls until the user has left, then stays quiet") {
  auto app = seeded();
  auto rings = [&] {
    auto events = app->dashboard->recent();
    return std::count_if(events.begin(), events.end(), [](auto& m) {
      return m["type"] == "call_state" && m["data"]["purpose"] == "departure_check" && m["data"]["state"] == "ringing";
    });
  };
  long long now = time::nowTs();
  app->queue->push({now, "u_demo", TaskKind::DEPARTURE_CHECK, {{"event_id", "evt_cse110_lec"}}});
  app->scheduler->tick(now);
  CHECK(rings() == 1);
  app->departure->onLocation("u_demo", 32.8776, -117.2373, 10, now);
  app->queue->push({now + 60, "u_demo", TaskKind::DEPARTURE_CHECK, {{"event_id", "evt_cse110_lec"}}});
  app->scheduler->tick(now + 60);
  CHECK(rings() == 1);
}

TEST_CASE("unknown users get ok=false") {
  auto app = seeded();
  CHECK_FALSE(app->departure->onLocation("nobody", 0, 0, 10, 0).ok);
}
