// T06: Urgency score (PRD 7.3).
#include <doctest/doctest.h>

#include "planner/Urgency.hpp"

using namespace enro;

TEST_CASE("weights load from event_types.json shape") {
  auto w = UrgencyWeights::fromConfig({{"urgency", {{"levels", {{"FIRM", 45}, {"URGENT", 75}}}}},
                                       {"types", {{"interview", {{"urgency_weight", 0.9}}}}}});
  CHECK(w.firm_at == 45);
  CHECK(w.type_weight["interview"] == doctest::Approx(0.9));
  UrgencyScorer s(w);
  auto r = s.score({});
  CHECK(r.score >= 0);
  CHECK(r.score <= 100);
}

TEST_CASE("TODO(T06): interview with little slack is URGENT (seed: 88)" * doctest::skip()) {}
TEST_CASE("TODO(T06): casual lunch with lots of slack is CHILL" * doctest::skip()) {}
TEST_CASE("TODO(T06): learned lateness bias raises the score" * doctest::skip()) {}
