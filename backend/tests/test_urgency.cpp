// T06: Urgency score (PRD 7.3).
#include <doctest/doctest.h>

#include "planner/Urgency.hpp"

using namespace enro;

namespace {
UrgencyInput input(const std::string& type, int slack, int events_today = 5, bool deadline = false, double bias = 0) {
  UrgencyInput in;
  in.event = {"evt", "u", "E", "", type, "calendar", 0, 0, false};
  in.slack_minutes = slack;
  in.events_today = events_today;
  in.has_deadline = deadline;
  in.bias.prep_bias = bias;
  return in;
}
}  // namespace

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

TEST_CASE("interview with little slack is URGENT") {
  UrgencyScorer s({});
  auto r = s.score(input("interview", 10, 5, true, 8));
  CHECK(r.level == Urgency::URGENT);
  CHECK(r.score >= 70);
  CHECK(r.reasoning.back().starts_with("urgency URGENT ("));
}

TEST_CASE("casual lunch with lots of slack is CHILL") {
  UrgencyScorer s({});
  auto r = s.score(input("casual", 300, 1));
  CHECK(r.level == Urgency::CHILL);
  CHECK(r.score < 40);
}

TEST_CASE("class planned ahead with a learned bias is FIRM") {
  UrgencyScorer s({});
  auto r = s.score(input("class", 120, 5, false, 12));
  CHECK(r.level == Urgency::FIRM);
}

TEST_CASE("learned lateness bias raises the score") {
  UrgencyScorer s({});
  CHECK(s.score(input("class", 60, 3, false, 12)).score > s.score(input("class", 60, 3, false, 0)).score);
}

TEST_CASE("less slack raises the score, and being late maxes it") {
  UrgencyScorer s({});
  CHECK(s.score(input("meeting", 30)).score > s.score(input("meeting", 200)).score);
  CHECK(s.score(input("meeting", -5)).score == s.score(input("meeting", 0)).score);
}

TEST_CASE("config type weight overrides the default and thresholds move levels") {
  UrgencyWeights w = UrgencyWeights::fromConfig({{"types", {{"casual", {{"urgency_weight", 1.0}}}}}});
  UrgencyScorer s(w);
  CHECK(s.score(input("casual", 0, 5, true)).level == Urgency::URGENT);
  w.urgent_at = 101;
  CHECK(UrgencyScorer(w).score(input("casual", 0, 5, true)).level == Urgency::FIRM);
}

TEST_CASE("score stays in 0..100 with one reasoning line per input") {
  UrgencyScorer s({});
  auto r = s.score(input("flight", -100, 50, true, 100));
  CHECK(r.score == 100);
  CHECK(r.reasoning.size() == 6);
}
