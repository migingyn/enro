// T13: Punctuality model (PRD 7.8).
#include <doctest/doctest.h>

#include "model/PunctualityModel.hpp"

using namespace enro;

namespace {
// Three weeks shaped like backend/seed/demo_history.json: class leaves ~12 min late, walks 15% slow,
// bus 20% slow, "five more minutes" turns into eleven.
nlohmann::json history() {
  nlohmann::json h = {{"outcomes", nlohmann::json::array()}, {"wake_ups", nlohmann::json::array()},
                      {"channel_events", nlohmann::json::array()}};
  long long day0 = 1791183600;  // 2026-10-05 00:00 PT
  int prep_noise[] = {10, 14, 12, 11, 13, 12};
  for (int d = 0; d < 18; ++d) {
    long long planned = day0 + d * 86400LL + 7 * 3600 + 40 * 60;
    long long leave = planned + prep_noise[d % 6] * 60;
    h["outcomes"].push_back({{"user_id", "u_demo"}, {"event_id", "c" + std::to_string(d)}, {"event_type", "class"},
                             {"mode", "walk"}, {"planned_leave", planned}, {"actual_leave", leave},
                             {"arrival", leave + 23 * 60}, {"predicted_travel_minutes", 20}});
  }
  for (int d = 0; d < 4; ++d) {
    long long planned = day0 + d * 86400LL + 10 * 3600;
    h["outcomes"].push_back({{"user_id", "u_demo"}, {"event_id", "i" + std::to_string(d)}, {"event_type", "interview"},
                             {"mode", "bus"}, {"planned_leave", planned}, {"actual_leave", planned + 8 * 60},
                             {"arrival", planned + 8 * 60 + 36 * 60}, {"predicted_travel_minutes", 30}});
  }
  int actual[] = {9, 12, 10, 11, 12};
  for (int d = 0; d < 5; ++d)
    h["wake_ups"].push_back({{"date", "2026-10-0" + std::to_string(5 + d)},
                             {"snooze_requests", {{{"requested_minutes", 5}, {"actual_minutes", actual[d]}}}}});
  for (int d = 0; d < 10; ++d)
    h["channel_events"].push_back({{"ts", day0 + d * 86400LL + 7 * 3600}, {"channel", "call"}, {"answered", d % 2 == 0}});
  return h;
}
}  // namespace

TEST_CASE("report is valid with no history") {
  PunctualityModel m;
  auto r = m.report("u_demo");
  CHECK(r.user_id == "u_demo");
  CHECK(m.forEventType("class").samples == 0);
  CHECK(m.forEventType("class").travel_factor == 1.0);
  CHECK(m.forEventType("class").snooze_inflation == 1.0);
}

TEST_CASE("seed history -> class prep_bias ~ +12 min") {
  PunctualityModel m;
  m.loadHistory(history());
  auto s = m.forEventType("class");
  CHECK(s.prep_bias == doctest::Approx(12).epsilon(0.1));
  CHECK(s.samples == 18);
}

TEST_CASE("walk travel_factor ~ 1.15, bus ~ 1.20") {
  PunctualityModel m;
  m.loadHistory(history());
  CHECK(m.forMode("walk").travel_factor == doctest::Approx(1.15).epsilon(0.02));
  CHECK(m.forMode("bus").travel_factor == doctest::Approx(1.20).epsilon(0.02));
}

TEST_CASE("snooze_inflation for class ~ 2.2") {
  PunctualityModel m;
  m.loadHistory(history());
  CHECK(m.forEventType("class").snooze_inflation == doctest::Approx(2.2).epsilon(0.1));
  CHECK(m.recentSnoozes("class").size() == 5);
}

TEST_CASE("headlines mention the biggest learned bias") {
  TransportLearner t;
  PunctualityModel m(&t);
  m.loadHistory(history());
  auto r = m.report("u_demo");
  REQUIRE(r.headlines.size() == 3);
  CHECK(r.headlines[0] == "You leave 12 minutes later than planned for class.");
  CHECK(r.headlines[1] == "Your walks take 15% longer than the map says.");
  CHECK(r.headlines[2] == "Last three times you said five more minutes, it was eleven.");
  CHECK_FALSE(r.transport.empty());
  REQUIRE(r.channel_response.size() == 1);
  CHECK(r.channel_response[0].time_of_day == "morning");
  CHECK(r.channel_response[0].answer_rate == doctest::Approx(0.5));
}
