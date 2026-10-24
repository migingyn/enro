// T15: Trip replay + commentator.
#include <doctest/doctest.h>

#include "TestApp.hpp"

using namespace enro;

TEST_CASE("missing trip file fails cleanly; commentator copies the position") {
  ReplayFeed feed("/nonexistent");
  CHECK_FALSE(feed.open("bus_trip_01"));
  auto app = test::fakeApp();
  auto tick = app->commentator->tick({"t", "201", "Q", 100}, {3, 50, 32.88, -117.21, "Stop", false});
  CHECK(tick.seq == 3);
  CHECK_FALSE(tick.on_route);
  CHECK_FALSE(app->trip_runner->start("does_not_exist", 10).ok);
}

TEST_CASE("TODO(T15): bus_trip_01 replays every position as a trip_tick" * doctest::skip()) {}
TEST_CASE("TODO(T15): detour segment produces exactly one REROUTE with a suggestion" * doctest::skip()) {}
TEST_CASE("TODO(T15): delay_minutes = eta - scheduled arrival" * doctest::skip()) {}
