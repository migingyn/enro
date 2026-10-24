// T13: TransportLearner (PRD 7.9) + PersonalityState.
#include <doctest/doctest.h>

#include "model/PersonalityState.hpp"
#include "model/TransportLearner.hpp"

using namespace enro;

TEST_CASE("distance buckets and preferred-mode default") {
  CHECK(TransportLearner::distanceBucket(500) == "<2km");
  CHECK(TransportLearner::distanceBucket(8000) == "5-15km");
  TransportLearner t;
  CHECK(t.predict("campus", 800, "walk") == "walk");
  User u;
  u.harshness = 0.4;
  CHECK(PersonalityState(u).current().harshness == doctest::Approx(0.4));
}

TEST_CASE("TODO(T13): most frequent mode wins, preferred breaks ties" * doctest::skip()) {}
TEST_CASE("TODO(T13): ignored calls nudge harshness up, clamped to 1" * doctest::skip()) {}
