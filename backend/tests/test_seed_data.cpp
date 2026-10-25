// S3: the demo config + seed data in backend/config and backend/seed load and produce the numbers
// the demo script relies on (contracts/examples/punctuality.json, PRD 14).
#include <doctest/doctest.h>

#include "TestApp.hpp"
#include "store/Seed.hpp"

using namespace enro;

namespace {
std::unique_ptr<AppContext> seededFromRepo() {
  auto app = test::fakeApp({{"SEED_DIR", Env::fromMap({}).seedDir()}});
  seedDemo(*app);
  return app;
}
}  // namespace

TEST_CASE("every config file is present and has its top-level keys") {
  auto app = test::fakeApp();
  CHECK(app->event_types.contains("types"));
  CHECK(app->event_types["types"]["interview"]["base_prep_minutes"] == 45);
  CHECK(app->personas["archetypes"].size() == 4);
  for (auto& a : app->personas["archetypes"]) CHECK(a.contains("voice"));
  CHECK(app->scripts["lines"].contains("wake_call_3"));
  CHECK(app->onboarding["questions"].size() == app->fixtures.json("onboarding_session")["questions"].size());
}

TEST_CASE("seed loads the demo user, a week of calendar, and three weeks of history") {
  auto app = seededFromRepo();
  auto user = app->repos->user("u_demo");
  REQUIRE(user);
  CHECK(user->name == "Sam Rivera");
  auto contacts = app->repos->contacts("u_demo");
  REQUIRE(contacts.size() == 2);
  CHECK(app->repos->count("events") >= 10);
  CHECK(app->repos->event("evt_cse110_dis"));   // demo-day ids match the fixtures
  CHECK(app->repos->count("outcomes") >= 25);
}

TEST_CASE("learned punctuality matches the demo story") {
  auto app = seededFromRepo();
  auto cls = app->punctuality->forEventType("class");
  CHECK(cls.prep_bias == doctest::Approx(12).epsilon(0.15));        // "You leave 12 minutes later than planned for class"
  CHECK(cls.snooze_inflation == doctest::Approx(2.2).epsilon(0.15));  // "said five, it was eleven"
  CHECK(app->punctuality->forMode("walk").travel_factor == doctest::Approx(1.15).epsilon(0.05));
  CHECK(app->punctuality->forMode("bus").travel_factor == doctest::Approx(1.20).epsilon(0.05));
  bool five_eleven = false;
  for (auto& h : app->punctuality->report("u_demo").headlines)
    five_eleven |= h.find("five") != std::string::npos && h.find("eleven") != std::string::npos;
  CHECK(five_eleven);
}

TEST_CASE("the demo email is held back until the email drop, and the bus trip has a detour") {
  auto app = seededFromRepo();
  for (auto& m : app->email->poll("u_demo")) CHECK(m.id != "msg_1003");
  CHECK(app->email->injectDemo("msg_1003"));
  auto feed = app->makeTransitFeed();
  REQUIRE(feed->open("bus_trip_01"));
  int off_route = 0;
  while (auto p = feed->next()) off_route += !p->on_route;
  CHECK(off_route > 0);
}

TEST_CASE("DEMO_USER_PHONE and DEMO_CONTACT_PHONE swap in real phones from .env, never from the repo") {
  auto app = test::fakeApp({{"DEMO_USER_PHONE", "+15551230001"}, {"DEMO_CONTACT_PHONE", "+15551230002"}});
  seedDemo(*app);
  CHECK(app->repos->user("u_demo")->phone == "+15551230001");
  bool confirmed_swapped = false;
  for (auto& c : app->repos->contacts("u_demo")) {
    if (c.consent == ConsentStatus::CONFIRMED) confirmed_swapped |= c.phone == "+15551230002";
    else CHECK(c.phone != "+15551230002");  // only the confirmed contact gets the real number
  }
  CHECK(confirmed_swapped);
  CHECK(seedDemo(*app).users == 0);  // reapplied on every boot, even when nothing is reseeded
  CHECK(app->repos->user("u_demo")->phone == "+15551230001");
}

TEST_CASE("DEMO_USER_PHONE may list several numbers; the first one is used") {
  auto app = test::fakeApp({{"DEMO_USER_PHONE", " +15551230009 , +15551230008"}});
  seedDemo(*app);
  CHECK(app->repos->user("u_demo")->phone == "+15551230009");
}
