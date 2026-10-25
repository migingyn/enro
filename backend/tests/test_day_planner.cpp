// T07: DayPlanner (today's events -> LeavePlans + wake block).
#include <doctest/doctest.h>

#include "TestApp.hpp"
#include "planner/DayPlanner.hpp"
#include "store/Seed.hpp"

using namespace enro;

TEST_CASE("demo day plan has one LeavePlan per event and day reasoning") {
  auto app = test::fakeApp();
  seedDemo(*app);
  DayPlan day = DayPlanner(*app).build("u_demo", "2026-10-26");
  REQUIRE_FALSE(day.events.empty());
  CHECK(day.plans.size() == day.events.size());
  CHECK(day.timezone == "America/Los_Angeles");
  for (size_t i = 0; i < day.plans.size(); ++i) {
    CHECK(day.plans[i].leave_by < day.events[i].start_ts);
    CHECK(day.plans[i].reasoning.size() == 5);
  }
  CHECK(day.reasoning[0].find("first at 08:00 (CSE 110 Lecture)") != std::string::npos);
  CHECK(day.reasoning[1].starts_with("Interview at 11:00 is the highest-stakes event (URGENT"));
}

TEST_CASE("later events start from the previous one, but the interview keeps its prep and goes by bus") {
  auto app = test::fakeApp();
  seedDemo(*app);
  DayPlan day = DayPlanner(*app).build("u_demo", "2026-10-26");
  auto plan = [&](const std::string& id) {
    for (auto& p : day.plans)
      if (p.event_id == id) return p;
    FAIL("no plan for " << id);
    return LeavePlan{};
  };
  CHECK(plan("evt_cse110_lec").prep_minutes == 20);
  CHECK(plan("evt_cse110_dis").prep_minutes == 0);
  CHECK(plan("evt_cse110_dis").reasoning[0] == "prep 0 min (already on campus after CSE 110 Lecture)");
  CHECK(plan("evt_interview").prep_minutes == 45);
  CHECK(plan("evt_interview").mode == "bus");
  CHECK(plan("evt_lunch").mode == "bus");  // lunch starts at the Qualcomm interview, ~5 km away (places.json)
}

TEST_CASE("default date is the next day with events, and consequences use the day's plan") {
  auto app = test::fakeApp();
  seedDemo(*app);
  DayPlanner planner(*app);
  CHECK(planner.defaultDate("u_demo") == "2026-10-26");
  auto r = planner.consequences("u_demo", 0);
  CHECK(r.worst == ConsequenceStatus::OK);
}
