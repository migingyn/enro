// T03: Google OAuth, Calendar, Gmail, Routes; seed sources.
#include <doctest/doctest.h>

#include "TestApp.hpp"
#include "google/CalendarClient.hpp"
#include "google/GoogleAuth.hpp"
#include "google/SeedSources.hpp"
#include "routing/Router.hpp"

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

TEST_CASE("authUrl has client_id, scopes, offline access, and a single-use state") {
  auto app = test::fakeApp();
  auto env = Env::fromMap({{"GOOGLE_CLIENT_ID", "cid.apps.googleusercontent.com"}, {"GOOGLE_CLIENT_SECRET", "s"},
                           {"PUBLIC_BASE_URL", "https://enro.example"}});
  GoogleAuth auth(env, *app->repos, [](const HttpRequest&) { return HttpResponse{500, ""}; });
  std::string url = auth.authUrl("u_demo");
  CHECK(url.rfind("https://accounts.google.com/o/oauth2/v2/auth?", 0) == 0);
  CHECK(url.find("client_id=cid.apps.googleusercontent.com") != std::string::npos);
  CHECK(url.find("access_type=offline") != std::string::npos);
  CHECK(url.find("gmail.readonly") != std::string::npos);
  auto pos = url.find("state=");
  REQUIRE(pos != std::string::npos);
  std::string state = url.substr(pos + 6, url.find('&', pos) - pos - 6);
  CHECK(auth.consumeState(state) == std::optional<std::string>("u_demo"));
  CHECK_FALSE(auth.consumeState(state).has_value());  // single use
}

TEST_CASE("calendar page keeps timed, located, confirmed events in the window") {
  long long now = 2000000000;
  auto at = [&](long long ts) { return nlohmann::json{{"dateTime", toRfc3339(ts)}}; };
  nlohmann::json page = {
      {"items",
       {{{"id", "a"}, {"status", "confirmed"}, {"summary", "CSE 110 Lecture"}, {"location", "Center Hall 101"},
         {"start", at(now + 3600)}, {"end", at(now + 7200)}},
        {{"id", "b"}, {"status", "cancelled"}, {"summary", "Gone"}, {"location", "x"}, {"start", at(now + 3600)},
         {"end", at(now + 7200)}},
        {{"id", "c"}, {"status", "confirmed"}, {"summary", "No place"}, {"start", at(now + 3600)},
         {"end", at(now + 7200)}},
        {{"id", "d"}, {"status", "confirmed"}, {"summary", "All day"}, {"location", "x"},
         {"start", {{"date", "2033-05-18"}}}, {"end", {{"date", "2033-05-19"}}}}}}};
  auto events = CalendarClient::parseEvents(page, "u_demo", now, now + CalendarClient::kWindowSeconds);
  REQUIRE(events.size() == 1);
  CHECK(events[0].title == "CSE 110 Lecture");
  CHECK(events[0].start_ts == now + 3600);
  CHECK(events[0].user_id == "u_demo");
}

TEST_CASE("GoogleRouter falls back on HTTP error") {
  FallbackRouter fallback;
  GoogleRouter router(Env::fromMap({{"GOOGLE_MAPS_API_KEY", "k"}}), fallback,
                      [](const HttpRequest&) { return HttpResponse{403, "{}"}; });
  Place a{"A", 32.88, -117.24}, b{"B", 32.90, -117.20};
  auto eta = router.eta("drive", a, b);
  CHECK(eta.source == "fallback");
  CHECK(eta.minutes == fallback.eta("drive", a, b).minutes);
}

TEST_CASE("GoogleRouter bus ETA is the ride only, since the planner adds the walks") {
  FallbackRouter fallback;
  nlohmann::json route = {
      {"routes",
       {{{"duration", "1800s"},
         {"distanceMeters", 9000},
         {"legs",
          {{{"steps",
             {{{"travelMode", "WALK"}, {"staticDuration", "300s"}},
              {{"travelMode", "TRANSIT"},
               {"staticDuration", "1080s"},
               {"transitDetails", {{"stopDetails", {{"departureTime", "2033-05-18T08:05:00Z"}}}}}},
              {{"travelMode", "WALK"}, {"staticDuration", "240s"}}}}}}}}}}};
  GoogleRouter router(Env::fromMap({{"GOOGLE_MAPS_API_KEY", "k"}}), fallback,
                      [&](const HttpRequest&) { return HttpResponse{200, route.dump()}; });
  Place a{"Home", 32.87, -117.22}, b{"Qualcomm", 32.9047, -117.1957};
  auto eta = router.eta("bus", a, b);
  CHECK(eta.source == "google");
  CHECK(eta.minutes == 21);  // 1800 s door to door - 540 s walking = 1260 s
  CHECK(eta.walk_to_minutes == 5);
  CHECK(eta.walk_from_minutes == 4);
  CHECK(router.eta("walk", a, b).minutes == 30);  // non-transit modes keep the full duration
}
