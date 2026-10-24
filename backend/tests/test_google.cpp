// T03: Google OAuth, Calendar, Gmail, Routes; seed sources.
#include <doctest/doctest.h>

#include "TestApp.hpp"
#include "google/SeedSources.hpp"

using namespace enro;

TEST_CASE("seed calendar falls back to plan.json and filters by range") {
  auto app = test::fakeApp();
  auto all = app->calendar->events("u_demo", 0, 4102444800);
  CHECK(all.size() >= 5);
  CHECK(app->calendar->events("someone_else", 0, 4102444800).empty());
}

TEST_CASE("seed inbox holds back the demo email until injectDemo") {
  SeedEmail inbox("/nonexistent");
  CHECK(inbox.poll("u_demo").empty());
  CHECK(inbox.injectDemo("msg_1003"));
  auto msgs = inbox.poll("u_demo");
  REQUIRE(msgs.size() == 1);
  CHECK(msgs[0].subject == "Section canceled today");
  CHECK(inbox.poll("u_demo").empty());  // delivered once
}

TEST_CASE("redirect URI matches the one registered in Google Cloud") {
  auto app = test::fakeApp({{"PUBLIC_BASE_URL", "https://enro.ngrok.app"}});
  CHECK(app->google_auth->redirectUri() == "https://enro.ngrok.app/oauth/google/callback");
}

TEST_CASE("TODO(T03): authUrl has client_id, scopes, offline access, state" * doctest::skip()) {}
TEST_CASE("TODO(T03): calendar incremental sync uses and saves syncToken" * doctest::skip()) {}
TEST_CASE("TODO(T03): gmail poll uses historyId and never stores bodies" * doctest::skip()) {}
TEST_CASE("TODO(T03): GoogleRouter falls back on HTTP error" * doctest::skip()) {}
