// T07: Ready-to-Leave Engine (PRD 7.4).
#include <doctest/doctest.h>

#include "planner/ReadyToLeave.hpp"

using namespace enro;

namespace {
// Returns a fixed ETA per mode so the numbers match contracts/examples/plan.json.
struct FixedRouter : Router {
  std::map<std::string, int> minutes;
  std::string name() const override { return "fixed"; }
  RouteEta eta(const std::string& mode, const Place&, const Place&) override { return {minutes[mode], 1000, mode, "fixed"}; }
};

nlohmann::json eventTypes() {
  return {{"types", {{"class", {{"base_prep_minutes", 20}, {"buffer_minutes", 5}}},
                     {"interview", {{"base_prep_minutes", 45}, {"buffer_minutes", 15}}}}}};
}

LeaveInput lectureInput() {
  LeaveInput in;
  in.user.timezone = "America/Los_Angeles";
  in.event = {"evt_cse110_lec", "u_demo", "CSE 110 Lecture", "Center Hall 105", "class", "calendar", 1793026800, 1793029800, false};
  in.mode = "walk";
  in.type_stats.prep_bias = 12;
  in.mode_stats.travel_factor = 1.15;
  in.events_today = 5;
  return in;
}
}  // namespace

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

TEST_CASE("CSE 110 lecture -> prep 32, travel 16, buffer 5, leave_by 07:39, reminder 07:07") {
  FixedRouter router;
  router.minutes["walk"] = 14;
  UrgencyScorer urgency({});
  ReadyToLeave rtl(eventTypes(), router, urgency);
  auto p = rtl.plan(lectureInput());
  CHECK(p.prep_minutes == 32);
  CHECK(p.travel_minutes == 16);
  CHECK(p.buffer_minutes == 5);
  CHECK(p.leave_by == 1793025540);
  CHECK(p.reminder_at == 1793023620);
  CHECK(p.urgency == Urgency::FIRM);
  REQUIRE(p.reasoning.size() == 5);
  CHECK(p.reasoning[0] == "prep 32 min = base 20 (class) + learned bias +12");
  CHECK(p.reasoning[1] == "travel 16 min = 14 min walk ETA x learned factor 1.15");
  CHECK(p.reasoning[2] == "leave_by 07:39 = start 08:00 - travel 16 - buffer 5");
  CHECK(p.reasoning[3] == "reminder_at 07:07 = leave_by 07:39 - prep 32");
  CHECK(p.reasoning[4].starts_with("urgency FIRM ("));
}

TEST_CASE("interview by bus -> travel 34 (28 x 1.20), buffer 15, leave_by 10:11") {
  FixedRouter router;
  router.minutes["bus"] = 28;
  UrgencyScorer urgency({});
  ReadyToLeave rtl(eventTypes(), router, urgency);
  LeaveInput in = lectureInput();
  in.event = {"evt_interview", "u_demo", "Qualcomm SWE Intern Interview", "6455 Lusk Blvd", "interview", "calendar",
              1793037600, 1793041200, false};
  in.mode = "bus";
  in.type_stats.prep_bias = 8;
  in.mode_stats.travel_factor = 1.20;
  auto p = rtl.plan(in);
  CHECK(p.prep_minutes == 53);
  CHECK(p.travel_minutes == 34);
  CHECK(p.buffer_minutes == 15);
  CHECK(p.leave_by == 1793034660);
  CHECK(p.reminder_at == 1793031480);
  CHECK(p.urgency == Urgency::URGENT);
  CHECK(p.reasoning[1] == "travel 34 min = 28 min bus ETA x learned factor 1.20");
  CHECK(p.reasoning[2] == "leave_by 10:11 = start 11:00 - travel 34 - buffer 15");
  CHECK(p.reasoning[3] == "reminder_at 09:18 = leave_by 10:11 - prep 53");
}

TEST_CASE("already on campus after the previous event -> prep 0, reminder equals leave_by") {
  FixedRouter router;
  router.minutes["walk"] = 6;
  UrgencyScorer urgency({});
  ReadyToLeave rtl(nlohmann::json::object(), router, urgency);
  LeaveInput in = lectureInput();
  in.event = {"evt_cse110_dis", "u_demo", "CSE 110 Discussion", "WLH 2005", "class", "calendar", 1793030400, 1793033400, false};
  in.already_out = true;
  in.after_title = "CSE 110 Lecture";
  auto p = rtl.plan(in);
  CHECK(p.prep_minutes == 0);
  CHECK(p.travel_minutes == 7);
  CHECK(p.reminder_at == p.leave_by);
  CHECK(p.reasoning[0] == "prep 0 min (already on campus after CSE 110 Lecture)");
}

TEST_CASE("missing config falls back to PRD defaults and an unknown factor counts as 1") {
  FixedRouter router;
  router.minutes["walk"] = 10;
  UrgencyScorer urgency({});
  ReadyToLeave rtl(nlohmann::json::object(), router, urgency);
  CHECK(rtl.basePrep("interview") == 45);
  CHECK(rtl.buffer("interview") == 15);
  CHECK(rtl.basePrep("unknown") == 20);
  LeaveInput in = lectureInput();
  in.mode_stats.travel_factor = 0;
  in.type_stats.prep_bias = -30;
  auto p = rtl.plan(in);
  CHECK(p.travel_minutes == 10);
  CHECK(p.prep_minutes == 0);
}
