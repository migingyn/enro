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

TEST_CASE("most frequent mode wins, preferred breaks ties") {
  TransportLearner t;
  for (int i = 0; i < 3; ++i) t.record("class", 1000, "walk");
  t.record("class", 1000, "bike");
  CHECK(t.predict("class", 900, "bike") == "walk");
  t.record("interview", 8000, "bus");
  t.record("interview", 8000, "drive");
  CHECK(t.predict("interview", 8000, "drive") == "drive");
  CHECK(t.predict("interview", 8000, "bus") == "bus");
  // No trips in this bucket yet: fall back to the type's overall habit, then to the preference.
  CHECK(t.predict("class", 20000, "drive") == "walk");
  CHECK(t.predict("flight", 20000, "drive") == "drive");
  auto counts = t.counts();
  REQUIRE(counts.size() == 2);
  CHECK(counts[0].destination_type == "class");
  CHECK(counts[0].predicted == "walk");
}
TEST_CASE("ignored calls nudge harshness up, clamped to 1") {
  User u;
  u.harshness = 0.97;
  PersonalityState s(u);
  s.nudge("ignored_call");
  CHECK(s.current().harshness == doctest::Approx(1.0));
  s.nudge("ignored_call");
  CHECK(s.current().harshness == doctest::Approx(1.0));
  s.nudge("asked_gentler");
  CHECK(s.current().harshness == doctest::Approx(0.92));
  s.nudge("unknown_signal");
  CHECK(s.current().harshness == doctest::Approx(0.92));
}

TEST_CASE("urgency shifts tone per message without changing the stored state") {
  User u;
  u.harshness = 0.4;
  u.verbosity = 0.5;
  PersonalityState s(u);
  auto urgent = s.forUrgency(Urgency::URGENT);
  CHECK(urgent.harshness == doctest::Approx(0.7));
  CHECK(urgent.verbosity == doctest::Approx(0.2));
  CHECK(s.forUrgency(Urgency::CHILL).harshness == doctest::Approx(0.4));
  CHECK(s.current().harshness == doctest::Approx(0.4));
}
