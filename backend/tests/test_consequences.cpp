// T08: Consequence Engine (PRD 7.5).
#include <doctest/doctest.h>

#include "planner/ConsequenceEngine.hpp"

using namespace enro;

namespace {
// Demo day from contracts/examples/plan.json.
std::vector<Event> demoEvents() {
  return {{"evt_cse110_lec", "u_demo", "CSE 110 Lecture", "", "class", "calendar", 1793026800, 1793029800, false},
          {"evt_cse110_dis", "u_demo", "CSE 110 Discussion", "", "class", "calendar", 1793030400, 1793033400, false},
          {"evt_interview", "u_demo", "Qualcomm SWE Intern Interview", "", "interview", "calendar", 1793037600, 1793041200, false}};
}

LeavePlan plan(const std::string& id, int buffer, long long leave_by) {
  LeavePlan p;
  p.event_id = id;
  p.buffer_minutes = buffer;
  p.leave_by = leave_by;
  return p;
}

std::vector<LeavePlan> demoPlans() {
  return {plan("evt_cse110_lec", 5, 1793025540), plan("evt_cse110_dis", 3, 1793029800), plan("evt_interview", 15, 1793034660)};
}
}  // namespace

TEST_CASE("evaluate returns one item per non-canceled event") {
  ConsequenceEngine ce;
  std::vector<Event> events = {{"a", "u", "A", "", "class", "calendar", 100, 200, false},
                               {"b", "u", "B", "", "class", "calendar", 300, 400, true}};
  auto r = ce.evaluate("u", events, {}, 10);
  CHECK(r.delay_minutes == 10);
  CHECK(r.items.size() == 1);
}

TEST_CASE("11 minute delay makes CSE 110 AT_RISK by 6 minutes, interview OK") {
  ConsequenceEngine ce;
  auto r = ce.evaluate("u_demo", demoEvents(), demoPlans(), 11);
  REQUIRE(r.items.size() == 3);
  CHECK(r.items[0].status == ConsequenceStatus::AT_RISK);
  CHECK(r.items[0].minutes_late == 6);
  CHECK(r.items[1].status == ConsequenceStatus::OK);
  CHECK(r.items[2].status == ConsequenceStatus::OK);
  CHECK(r.worst == ConsequenceStatus::AT_RISK);
  CHECK(r.summary == "11 more minutes makes you 6 minutes late to CSE 110 Lecture. The interview is still safe.");
}

TEST_CASE("a delay inside the buffer is OK everywhere") {
  ConsequenceEngine ce;
  auto r = ce.evaluate("u_demo", demoEvents(), demoPlans(), 5);
  CHECK(r.worst == ConsequenceStatus::OK);
  CHECK(r.summary == "5 more minutes is fine. You're still on time for everything.");
}

TEST_CASE("large delay marks the event MISSED") {
  ConsequenceEngine ce;
  auto r = ce.evaluate("u_demo", demoEvents(), demoPlans(), 40);
  CHECK(r.items[0].status == ConsequenceStatus::MISSED);
  CHECK(r.worst == ConsequenceStatus::MISSED);
  CHECK(r.summary.starts_with("40 more minutes means you miss CSE 110 Lecture."));
}

TEST_CASE("interview is MISSED after 15 minutes late, not half its length") {
  ConsequenceEngine ce;
  std::vector<Event> ev = {demoEvents()[2]};
  CHECK(ce.evaluate("u", ev, {demoPlans()[2]}, 25).items[0].status == ConsequenceStatus::AT_RISK);
  CHECK(ce.evaluate("u", ev, {demoPlans()[2]}, 30).items[0].status == ConsequenceStatus::MISSED);
}

TEST_CASE("delay cascades to back-to-back events") {
  ConsequenceEngine ce;
  // 60 minutes late to the 08:00 lecture means arriving 08:55, 5 minutes after leave_by 08:50 for the discussion.
  auto r = ce.evaluate("u_demo", demoEvents(), demoPlans(), 60);
  CHECK(r.items[0].status == ConsequenceStatus::MISSED);
  CHECK(r.items[1].minutes_late == 2);  // carry 5 - buffer 3
  CHECK(r.items[1].status == ConsequenceStatus::AT_RISK);
  CHECK(r.items[2].status == ConsequenceStatus::OK);
}

TEST_CASE("canceled events are skipped, and events without a plan use a 5 minute buffer") {
  ConsequenceEngine ce;
  auto ev = demoEvents();
  ev[0].canceled = true;
  auto r = ce.evaluate("u_demo", ev, {}, 8);
  REQUIRE(r.items.size() == 2);
  CHECK(r.items[0].event_id == "evt_cse110_dis");
  CHECK(r.items[0].minutes_late == 3);
}
