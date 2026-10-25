// T15: Trip replay + commentator.
#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

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

namespace {
// The real seed trip, played tick by tick through the commentator.
std::vector<TripTick> playSeedTrip(AppContext& app, TripInfo* info_out = nullptr) {
  ReplayFeed feed(Env::fromMap({}).seedDir());
  REQUIRE(feed.open("bus_trip_01"));
  TripInfo info = feed.info();
  if (info_out) *info_out = info;
  app.commentator->reset();
  std::vector<TripTick> ticks;
  while (auto p = feed.next()) ticks.push_back(app.commentator->tick(info, *p));
  return ticks;
}
}  // namespace

TEST_CASE("bus_trip_01 replays every position as a trip_tick") {
  auto app = test::fakeApp({{"SEED_DIR", Env::fromMap({}).seedDir()}});
  auto run = app->trip_runner->start("bus_trip_01", 100000);  // ~0 s between ticks
  REQUIRE(run.ok);
  CHECK(run.ticks == 34);
  for (int i = 0; i < 500 && app->trip_runner->running(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
  app->trip_runner->stop();

  int ticks = 0, decisions = 0, last_seq = -1;
  bool ordered = true;
  for (auto& e : app->dashboard->recent()) {
    if (e["type"] == "trip_tick") {
      ++ticks;
      ordered = ordered && e["data"]["seq"].get<int>() > last_seq;
      last_seq = e["data"]["seq"].get<int>();
    }
    if (e["type"] == "decision" && e["data"]["kind"] == "transit_speak") ++decisions;
  }
  CHECK(ticks == 34);
  CHECK(ordered);
  CHECK(decisions > 0);  // speaking writes a Decision row
}

TEST_CASE("detour segment produces exactly one REROUTE with a suggestion") {
  auto app = test::fakeApp();
  auto ticks = playSeedTrip(*app);
  int reroutes = 0;
  for (auto& t : ticks) {
    if (t.verdict != "REROUTE") {
      CHECK_FALSE(t.reroute.has_value());
      continue;
    }
    ++reroutes;
    CHECK(t.seq == 15);  // first off-route position in the seed
    CHECK_FALSE(t.on_route);
    REQUIRE(t.reroute.has_value());
    CHECK_FALSE(t.reroute->suggestion.empty());
    CHECK(t.reroute->new_eta_ts > t.ts);
    CHECK_FALSE(t.line.empty());  // replay speaks
  }
  CHECK(reroutes == 1);

  // A second replay on the same commentator reroutes again.
  auto again = playSeedTrip(*app);
  CHECK(std::count_if(again.begin(), again.end(), [](const TripTick& t) { return t.verdict == "REROUTE"; }) == 1);
}

TEST_CASE("delay_minutes = eta - scheduled arrival") {
  auto app = test::fakeApp();
  TripInfo info;
  auto ticks = playSeedTrip(*app, &info);
  for (auto& t : ticks) {
    CHECK(t.scheduled_arrival_ts == info.scheduled_arrival_ts);
    CHECK(t.delay_minutes == static_cast<int>(std::llround((t.eta_ts - t.scheduled_arrival_ts) / 60.0)));
  }
  CHECK(ticks.front().delay_minutes == 0);           // leaves on schedule
  CHECK(ticks.back().eta_ts == ticks.back().ts);     // at the destination, ETA is now
  CHECK(ticks.back().delay_minutes == 3);            // the recording arrives 3 minutes late
  int worst = 0;
  for (auto& t : ticks) worst = std::max(worst, t.delay_minutes);
  CHECK(worst > 0);

  // Without planned stops the ETA falls back to the schedule.
  CHECK(Commentator::estimateEta({"t", "201", "Q", 100}, {0, 50, 1, 1, "", true}) == 100);
}
