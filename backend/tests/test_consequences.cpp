// T08: Consequence Engine (PRD 7.5).
#include <doctest/doctest.h>

#include "planner/ConsequenceEngine.hpp"

using namespace enro;

TEST_CASE("evaluate returns one item per non-canceled event") {
  ConsequenceEngine ce;
  std::vector<Event> events = {{"a", "u", "A", "", "class", "calendar", 100, 200, false},
                               {"b", "u", "B", "", "class", "calendar", 300, 400, true}};
  auto r = ce.evaluate("u", events, {}, 10);
  CHECK(r.delay_minutes == 10);
  CHECK(r.items.size() == 1);
}

// Expected numbers: contracts/examples/consequences.json
TEST_CASE("TODO(T08): 11 minute delay makes CSE 110 AT_RISK by 6 minutes, interview OK" * doctest::skip()) {}
TEST_CASE("TODO(T08): large delay marks the event MISSED" * doctest::skip()) {}
TEST_CASE("TODO(T08): delay cascades to back-to-back events" * doctest::skip()) {}
