// T13: Punctuality model (PRD 7.8).
#include <doctest/doctest.h>

#include "model/PunctualityModel.hpp"

using namespace enro;

TEST_CASE("report is valid with no history") {
  PunctualityModel m;
  auto r = m.report("u_demo");
  CHECK(r.user_id == "u_demo");
  CHECK(m.forEventType("class").samples == 0);
}

// Expected values: contracts/examples/punctuality.json, from backend/seed/demo_history.json
TEST_CASE("TODO(T13): seed history -> class prep_bias ~ +12 min" * doctest::skip()) {}
TEST_CASE("TODO(T13): walk travel_factor ~ 1.15, bus ~ 1.20" * doctest::skip()) {}
TEST_CASE("TODO(T13): snooze_inflation for class ~ 2.2" * doctest::skip()) {}
TEST_CASE("TODO(T13): headlines mention the biggest learned bias" * doctest::skip()) {}
