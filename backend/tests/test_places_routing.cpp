// T07 follow-up: DayPlanner routes with coordinates from backend/config/places.json, so distant events
// (the Qualcomm interview) get real travel times instead of the 1500 m default.
#include <doctest/doctest.h>

#include "TestApp.hpp"
#include "planner/DayPlanner.hpp"
#include "store/Seed.hpp"

using namespace enro;

TEST_CASE("the interview at Lusk Blvd is routed from home by real distance, not the default") {
  auto app = test::fakeApp();
  seedDemo(*app);
  DayPlan day = DayPlanner(*app).build("u_demo", "2026-10-26");
  const LeavePlan* interview = nullptr;
  for (auto& p : day.plans)
    if (p.event_id == "evt_interview") interview = &p;
  REQUIRE(interview);
  // Home (Nobel Dr) -> Qualcomm Building Q is ~4.9 km straight line; by bus at 18 km/h that's ~16 min before
  // the learned factor. The 1500 m default would give ~5 min.
  CHECK(interview->travel_minutes >= 14);
  CHECK(interview->leave_by <= app->repos->event("evt_interview")->start_ts - (14 + 15) * 60);
}

TEST_CASE("unknown locations still plan with the default distance") {
  auto app = test::fakeApp();
  seedDemo(*app);
  Event e{"evt_x", "u_demo", "Somewhere", "An unlisted place", "casual", "calendar", 1793055600, 1793059200, false};
  app->repos->upsertEvent(e);
  DayPlan day = DayPlanner(*app).build("u_demo", "2026-10-26");
  bool found = false;
  for (auto& p : day.plans) found |= p.event_id == "evt_x" && p.travel_minutes > 0;
  CHECK(found);
}

TEST_CASE("a long trip is never planned as a walk (lunch after the Qualcomm interview)") {
  auto app = test::fakeApp();
  seedDemo(*app);
  DayPlan day = DayPlanner(*app).build("u_demo", "2026-10-26");
  for (auto& p : day.plans)
    if (p.event_id == "evt_lunch") CHECK(p.mode != "walk");
}
