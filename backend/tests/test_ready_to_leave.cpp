// T07: Ready-to-Leave Engine (PRD 7.4).
#include <doctest/doctest.h>

#include "planner/ReadyToLeave.hpp"

using namespace enro;

TEST_CASE("plan returns a valid LeavePlan for the event") {
  FallbackRouter router;
  UrgencyScorer urgency({});
  ReadyToLeave rtl(nlohmann::json::object(), router, urgency);
  LeaveInput in;
  in.event = {"evt", "u", "CSE 110 Lecture", "Center Hall", "class", "calendar", 1793026800, 1793029800, false};
  auto p = rtl.plan(in);
  CHECK(p.event_id == "evt");
  CHECK(p.leave_by < in.event.start_ts);
  CHECK(p.reminder_at <= p.leave_by);
  CHECK_FALSE(p.reasoning.empty());
}

// Expected numbers: contracts/examples/plan.json
TEST_CASE("TODO(T07): CSE 110 lecture -> prep 32, travel 16, buffer 5, leave_by 07:39, reminder 07:07" * doctest::skip()) {}
TEST_CASE("TODO(T07): interview by bus -> travel 34 (28 x 1.20), buffer 15, leave_by 10:11" * doctest::skip()) {}
TEST_CASE("TODO(T07): reasoning has one line per step of 7.4" * doctest::skip()) {}
