// T11: Conversation + negotiation.
#include <doctest/doctest.h>

#include "TestApp.hpp"
#include "store/Seed.hpp"

using namespace enro;

TEST_CASE("answer then speech records agent and user turns") {
  auto app = test::fakeApp();
  seedDemo(*app);
  auto id = app->wake_ladder->start("u_demo");
  CHECK(app->conversation->onEvent(id, "answered", ""));
  CHECK(app->conversation->onEvent(id, "speech", "Give me five more minutes."));
  auto turns = app->repos->turns(id);
  REQUIRE(turns.size() == 3);
  CHECK(turns[1].speaker == "user");
  CHECK(app->repos->call(id)->state == "in_progress");
}

TEST_CASE("unknown call ids are ignored") {
  auto app = test::fakeApp();
  CHECK_FALSE(app->conversation->onEvent("nope", "answered", ""));
}

TEST_CASE("TODO(T11): 'five more minutes' is countered with history ('last three times ... eleven')" * doctest::skip()) {}
TEST_CASE("TODO(T11): snooze GRANT when consequences stay OK" * doctest::skip()) {}
TEST_CASE("TODO(T11): AWAKE passes the awake check and ends the call" * doctest::skip()) {}
