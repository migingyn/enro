// T07/T13: walking legs in bus trips, and learning from real trips detected by the Beacon page.
#include <doctest/doctest.h>

#include "TestApp.hpp"
#include "planner/DayPlanner.hpp"
#include "store/Seed.hpp"
#include "util/Time.hpp"

using namespace enro;

namespace {
const LeavePlan& planFor(const DayPlan& day, const std::string& id) {
  for (auto& p : day.plans)
    if (p.event_id == id) return p;
  FAIL("no plan for " << id);
  return day.plans.front();
}
}  // namespace

TEST_CASE("a bus trip includes walking to and from the stop") {
  auto app = test::fakeApp();
  seedDemo(*app);
  DayPlan day = DayPlanner(*app).build("u_demo", "2026-10-26");
  const LeavePlan& interview = planFor(day, "evt_interview");
  REQUIRE(interview.mode == "bus");
  bool legs = false;
  for (auto& line : interview.reasoning) legs |= line.find("walk") != std::string::npos && line.find("bus") != std::string::npos;
  CHECK(legs);
  // walk 6 + ride ~16 + walk 4 before factors: comfortably more than the ride alone.
  CHECK(interview.travel_minutes >= 24);
}

TEST_CASE("a walking trip has no extra legs") {
  auto app = test::fakeApp();
  seedDemo(*app);
  DayPlan day = DayPlanner(*app).build("u_demo", "2026-10-26");
  const LeavePlan& lecture = planFor(day, "evt_cse110_lec");
  REQUIRE(lecture.mode == "walk");
  for (auto& line : lecture.reasoning) CHECK(line.find("walk to stop") == std::string::npos);
}

TEST_CASE("leaving home then arriving at the event records a real walking trip and updates the model") {
  auto app = test::fakeApp();
  seedDemo(*app);
  int before_outcomes = app->repos->count("outcomes");
  int before_walks = app->punctuality->forMode("walk").samples;
  long long t0 = 1793025600;  // 07:40 on demo day, just after the lecture's leave_by
  // Depart: ~640 m from home on the way to Center Hall.
  CHECK(app->departure->onLocation("u_demo", 32.8740, -117.2330, 10, t0).departed);
  CHECK(app->repos->count("outcomes") == before_outcomes);  // not arrived yet
  // Arrive at Center Hall 19 minutes later.
  app->departure->onLocation("u_demo", 32.8776, -117.2373, 10, t0 + 19 * 60);
  REQUIRE(app->repos->count("outcomes") == before_outcomes + 1);
  auto o = app->repos->outcomes("u_demo").back();
  CHECK(o.event_id == "evt_cse110_lec");
  CHECK(o.mode == "walk");
  CHECK(o.actual_leave == t0);
  CHECK(o.arrival == t0 + 19 * 60);
  CHECK(app->punctuality->forMode("walk").samples == before_walks + 1);
  // A second fix at the venue doesn't record the trip twice.
  app->departure->onLocation("u_demo", 32.8776, -117.2373, 10, t0 + 20 * 60);
  CHECK(app->repos->count("outcomes") == before_outcomes + 1);
}
