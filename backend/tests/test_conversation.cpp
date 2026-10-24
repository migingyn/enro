// T11: Conversation + negotiation.
#include <doctest/doctest.h>

#include "TestApp.hpp"
#include "store/Seed.hpp"

using namespace enro;

namespace {
// "Five more minutes" has turned into ten to twelve the last three mornings.
void loadSnoozeHistory(AppContext& app) {
  nlohmann::json wake_ups = nlohmann::json::array();
  for (int actual : {12, 10, 11, 12})
    wake_ups.push_back({{"snooze_requests", {{{"requested_minutes", 5}, {"actual_minutes", actual}}}}});
  app.punctuality->loadHistory({{"wake_ups", wake_ups}});
}

std::string lastAgentLine(AppContext& app, const std::string& call_id) {
  auto turns = app.repos->turns(call_id);
  return turns.empty() ? "" : turns.back().text;
}
}  // namespace

TEST_CASE("answer then speech records agent and user turns") {
  auto app = test::fakeApp();
  seedDemo(*app);
  auto id = app->wake_ladder->start("u_demo");
  CHECK(app->conversation->onEvent(id, "answered", ""));
  CHECK(app->conversation->onEvent(id, "speech", "Give me five more minutes."));
  auto turns = app->repos->turns(id);
  REQUIRE(turns.size() == 3);
  CHECK(turns[1].speaker == "user");
  CHECK(turns[1].intent == "SNOOZE_REQUEST");
  CHECK(app->repos->call(id)->state == "in_progress");
}

TEST_CASE("unknown call ids are ignored") {
  auto app = test::fakeApp();
  CHECK_FALSE(app->conversation->onEvent("nope", "answered", ""));
}

TEST_CASE("the opener names the first event and what ten more minutes costs") {
  auto app = test::fakeApp();
  seedDemo(*app);
  auto id = app->wake_ladder->start("u_demo");
  auto line = app->conversation->onEvent(id, "answered", "");
  REQUIRE(line);
  CHECK(line->text.find("CSE 110 Lecture is at 08:00") != std::string::npos);
  CHECK(line->text.find("10 more minutes") != std::string::npos);
}

TEST_CASE("'five more minutes' is countered with history ('last three times ... eleven')") {
  auto app = test::fakeApp();
  seedDemo(*app);
  loadSnoozeHistory(*app);
  auto id = app->wake_ladder->start("u_demo");
  app->conversation->onEvent(id, "answered", "");
  auto line = app->conversation->onEvent(id, "speech", "Give me five more minutes.");
  REQUIRE(line);
  CHECK(line->text.starts_with("Last three times you said five, it was eleven."));
  CHECK(line->text.find("6 minutes late to CSE 110 Lecture") != std::string::npos);
  CHECK_FALSE(line->end_call);

  auto d = app->conversation->negotiate("u_demo", 5);
  CHECK(d.verdict == "COUNTER");
  CHECK(d.effective == 11);
  CHECK(d.counter > 0);
  CHECK(d.counter < 5);
}

TEST_CASE("snooze GRANT when consequences stay OK") {
  auto app = test::fakeApp();
  seedDemo(*app);
  auto id = app->wake_ladder->start("u_demo");
  app->conversation->onEvent(id, "answered", "");
  auto line = app->conversation->onEvent(id, "speech", "five more minutes");
  REQUIRE(line);
  CHECK(line->end_call);
  CHECK(app->conversation->negotiate("u_demo", 5).verdict == "GRANT");
}

TEST_CASE("a snooze that misses the first event is REFUSED") {
  auto app = test::fakeApp();
  seedDemo(*app);
  CHECK(app->conversation->negotiate("u_demo", 45).verdict == "REFUSE");
}

TEST_CASE("AWAKE passes the awake check and ends the call") {
  auto app = test::fakeApp();
  seedDemo(*app);
  auto id = app->wake_ladder->start("u_demo");
  app->conversation->onEvent(id, "answered", "");
  auto check = app->conversation->onEvent(id, "speech", "Okay okay, I'm up");
  REQUIRE(check);
  CHECK_FALSE(check->end_call);
  CHECK(check->text.find("first thing today") != std::string::npos);
  CHECK_FALSE(app->conversation->onEvent(id, "speech", "ugh")->end_call);
  auto done = app->conversation->onEvent(id, "speech", "CSE 110 at eight");
  REQUIRE(done);
  CHECK(done->end_call);
}

TEST_CASE("LEFT ends the call") {
  auto app = test::fakeApp();
  seedDemo(*app);
  auto id = app->wake_ladder->start("u_demo");
  app->conversation->onEvent(id, "answered", "");
  CHECK(app->conversation->onEvent(id, "speech", "I'm leaving now")->end_call);
}
