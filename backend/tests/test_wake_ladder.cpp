// T10: Wake ladder (PRD 7.6).
#include <doctest/doctest.h>

#include "TestApp.hpp"
#include "store/Seed.hpp"
#include "util/Time.hpp"

using namespace enro;

namespace {
std::unique_ptr<AppContext> seededApp() {
  auto app = test::fakeApp({{"LADDER_DEMO_GAP_SECONDS", "15"}});
  seedDemo(*app);
  return app;
}
int callsFor(AppContext& app, const std::string& user) {
  auto events = app.dashboard->recent();  // one copy: begin/end must come from the same vector
  return static_cast<int>(std::count_if(events.begin(), events.end(), [&](auto& m) {
    return m["type"] == "call_state" && m["data"]["user_id"] == user && m["data"]["state"] == "ringing";
  }));
}
}  // namespace

TEST_CASE("start rings CALL_1 through the voice provider and records the call") {
  auto app = seededApp();
  auto id = app->wake_ladder->start("u_demo");
  auto call = app->repos->call(id);
  REQUIRE(call);
  CHECK(call->purpose == "wake");
  CHECK(call->attempt == 1);
  CHECK(call->provider_sid == "SIM-" + id);
}

TEST_CASE("plan() steps match contracts/examples/plan.json wake block") {
  auto app = seededApp();
  auto user = *app->repos->user("u_demo");
  auto w = app->wake_ladder->plan(user, "2026-10-26");
  auto expected = app->fixtures.json("plan")["wake"];
  CHECK(w.alarm_ts == expected["alarm_ts"].get<long long>());
  REQUIRE(w.steps.size() == 4);
  for (size_t i = 0; i < 4; ++i) {
    CHECK(w.steps[i].step == expected["steps"][i]["step"].get<std::string>());
    CHECK(w.steps[i].at == expected["steps"][i]["at"].get<long long>());
    CHECK(w.steps[i].tone == expected["steps"][i]["tone"].get<std::string>());
    CHECK(w.steps[i].notify_contacts == expected["steps"][i]["notify_contacts"].get<bool>());
  }
}

TEST_CASE("unanswered calls escalate CALL_1 -> CALL_2 -> CALL_3, and CALL_3 texts confirmed contacts") {
  auto app = seededApp();
  long long t0 = time::nowTs();
  app->wake_ladder->start("u_demo");
  CHECK(callsFor(*app, "u_demo") == 1);
  app->scheduler->tick(t0 + 15);
  CHECK(callsFor(*app, "u_demo") == 2);
  CHECK(app->sim->sentSms().empty());
  app->scheduler->tick(t0 + 30);
  CHECK(callsFor(*app, "u_demo") == 3);
  auto sms = app->sim->sentSms();
  REQUIRE(sms.size() == 1);  // Mom is CONFIRMED, Jules is PENDING
  CHECK(sms[0]["to"] == "+16195554417");
  app->scheduler->tick(t0 + 10000);  // ladder ends after CALL_3
  CHECK(callsFor(*app, "u_demo") == 3);
}

TEST_CASE("a call in progress defers the next ladder call; hanging up without the awake check resumes it") {
  auto app = seededApp();
  long long t0 = time::nowTs();
  auto id = app->wake_ladder->start("u_demo");
  app->conversation->onEvent(id, "answered", "");
  app->scheduler->tick(t0 + 15);
  CHECK(callsFor(*app, "u_demo") == 1);  // still talking: deferred
  app->conversation->onEvent(id, "completed", "");
  app->scheduler->tick(t0 + 30);
  CHECK(callsFor(*app, "u_demo") == 2);  // hung up without passing the awake check
}

TEST_CASE("declining pulls the next call forward, and the old scheduled task is ignored") {
  auto app = seededApp();
  long long t0 = time::nowTs();
  auto id = app->wake_ladder->start("u_demo");
  app->conversation->onEvent(id, "declined", "");
  app->scheduler->tick(t0 + 5);  // retry fires early in demo mode
  CHECK(callsFor(*app, "u_demo") == 2);
  app->scheduler->tick(t0 + 15);  // the original CALL_2 task is stale; CALL_3 comes 15 s after CALL_2
  CHECK(callsFor(*app, "u_demo") == 2);
  app->scheduler->tick(t0 + 25);
  CHECK(callsFor(*app, "u_demo") == 3);
}

TEST_CASE("onAwake stops the ladder and schedules the 5 minute verification call") {
  auto app = seededApp();
  long long t0 = time::nowTs();
  auto id = app->wake_ladder->start("u_demo");
  app->wake_ladder->onAwake(id);
  app->scheduler->tick(t0 + 60);
  CHECK(callsFor(*app, "u_demo") == 1);  // no more ladder calls
  app->scheduler->tick(t0 + 300);
  CHECK(callsFor(*app, "u_demo") == 2);  // AWAKE_VERIFY
  auto last = app->repos->call(app->wake_ladder->lastCallId("u_demo"));
  REQUIRE(last);
  CHECK(last->purpose == "awake_verify");
}

TEST_CASE("the contact SMS names the time of the first thing in the day's plan") {
  auto app = seededApp();
  long long t0 = time::nowTs();
  app->wake_ladder->start("u_demo");
  app->scheduler->tick(t0 + 15);
  app->scheduler->tick(t0 + 30);
  auto sms = app->sim->sentSms();
  REQUIRE(sms.size() == 1);
  std::string body = sms[0]["body"];
  CHECK(body.find("something at 8:00") != std::string::npos);
}
