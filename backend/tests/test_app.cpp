// app/: provider selection from env (PRD 12). Shared.
#include <doctest/doctest.h>

#include "TestApp.hpp"

using namespace enro;

TEST_CASE("empty env selects every fake provider") {
  auto app = test::fakeApp();
  CHECK(app->providers.voice == "sim");
  CHECK(app->providers.llm == "fake");
  CHECK(app->providers.reflex == "rules");
  CHECK(app->providers.calendar == "seed");
  CHECK(app->providers.email == "seed");
  CHECK(app->providers.router == "fallback");
  CHECK(app->providers.transit == "replay");
  CHECK(app->sim != nullptr);
}

TEST_CASE("real providers are selectable without keys and never crash") {
  auto app = test::fakeApp({{"VOICE_PROVIDER", "twilio"}, {"LLM_PROVIDER", "openai"}, {"REFLEX_PROVIDER", "jev"},
                            {"CALENDAR_SOURCE", "google"}, {"EMAIL_SOURCE", "gmail"}, {"GOOGLE_MAPS_API_KEY", "x"}});
  CHECK(app->providers.voice == "twilio");
  CHECK(app->providers.llm == "openai");
  CHECK(app->providers.reflex == "jev");
  CHECK(app->providers.calendar == "google");
  CHECK(app->providers.email == "gmail");
  CHECK(app->providers.router == "google");
  CHECK(app->sim == nullptr);
}

TEST_CASE("commented-out alternates fall back to fakes") {
  auto app = test::fakeApp({{"VOICE_PROVIDER", "retell"}, {"LLM_PROVIDER", "anthropic"}});
  CHECK(app->providers.voice == "sim");
  CHECK(app->providers.llm == "fake");
}
