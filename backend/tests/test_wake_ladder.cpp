// T10: Wake ladder (PRD 7.6).
#include <doctest/doctest.h>

#include "TestApp.hpp"
#include "store/Seed.hpp"

using namespace enro;

TEST_CASE("start rings CALL_1 through the voice provider and records the call") {
  auto app = test::fakeApp();
  seedDemo(*app);
  auto id = app->wake_ladder->start("u_demo");
  auto call = app->repos->call(id);
  REQUIRE(call);
  CHECK(call->purpose == "wake");
  CHECK(call->attempt == 1);
  CHECK(call->provider_sid == "SIM-" + id);
}

TEST_CASE("TODO(T10): CALL_1 at alarm+grace, CALL_2 +3, CALL_3 +3 with contacts notified" * doctest::skip()) {}
TEST_CASE("TODO(T10): answering any call stops escalation and schedules AWAKE_VERIFY +5" * doctest::skip()) {}
TEST_CASE("TODO(T10): plan() steps match contracts/examples/plan.json wake block" * doctest::skip()) {}
