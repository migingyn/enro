// T01: /api/onboarding/complete saves the spoken profile and PENDING contacts (PRD 5, 7.7).
#include <doctest/doctest.h>

#include "TestApp.hpp"
#include "http/OnboardingRoutes.hpp"
#include "models/Json.hpp"

using namespace enro;

TEST_CASE("onboarding complete saves user + PENDING contacts") {
  auto app = test::fakeApp();
  auto body = app->fixtures.json("onboarding_complete_request");
  auto res = completeOnboarding(*app, body, 1792825200);

  auto u = app->repos->user("u_demo");
  REQUIRE(u.has_value());
  CHECK(u->name == "Sam Rivera");
  CHECK(u->phone == "+18585550142");
  CHECK(u->home_area == "La Jolla, Nobel Dr near UC San Diego");
  CHECK(u->alarm_time == "07:00");
  CHECK(u->preferred_mode["off_campus"] == "bus");
  CHECK(u->prep_minutes["formal"] == 40);
  CHECK(u->harshness == doctest::Approx(0.4));
  CHECK(u->archetype_id == "best_friend");  // closest harshness to 0.4, ties go gentler

  auto contacts = app->repos->contacts("u_demo");
  REQUIRE(contacts.size() == 2);
  for (auto& c : contacts) CHECK(c.consent == ConsentStatus::PENDING);

  CHECK(res["user"]["id"] == "u_demo");
  CHECK(res["contacts"].size() == 2);
  CHECK(res["oauth_url"] == "/oauth/google/start?user_id=u_demo");
}

TEST_CASE("onboarding never upgrades consent, and keeps a contact's existing consent") {
  auto app = test::fakeApp();
  auto body = app->fixtures.json("onboarding_complete_request");
  completeOnboarding(*app, body, 100);
  auto mom = app->repos->contacts("u_demo")[0];
  mom.consent = ConsentStatus::CONFIRMED;  // as if Mom replied YES
  app->repos->upsertContact(mom);
  completeOnboarding(*app, body, 200);  // the user redoes onboarding
  CHECK(app->repos->contact(mom.id)->consent == ConsentStatus::CONFIRMED);
  CHECK(app->repos->contacts("u_demo").size() == 2);  // no duplicates
}

TEST_CASE("harsh answers pick a harsher persona; a new id outside demo mode") {
  auto app = test::fakeApp({{"DEMO_MODE", "false"}});
  nlohmann::json body = {{"profile", {{"name", "Alex"}, {"harshness_pref", 0.95}}}, {"contacts", nlohmann::json::array()}};
  auto res = completeOnboarding(*app, body, 1234);
  CHECK(res["user"]["id"] == "u_1234");
  CHECK(res["user"]["archetype_id"] == "drill_sergeant");
  CHECK(app->repos->user("u_1234")->timezone == "America/Los_Angeles");
}
