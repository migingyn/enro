// T01: OpenAIClient + FakeLlm.
#include <doctest/doctest.h>

#include "ai/FakeLlm.hpp"
#include "ai/OpenAIClient.hpp"

using namespace enro;

TEST_CASE("FakeLlm returns schema-shaped JSON and a fake realtime key") {
  FakeLlm llm;
  auto j = llm.chatJson("sys", "user", {{"properties", {{"label", {{"type", "string"}}}, {"minutes", {{"type", "integer"}}}}}});
  CHECK(j["label"] == "");
  CHECK(j["minutes"] == 0);
  CHECK(llm.realtimeSession("hi").client_secret == "fake-ephemeral-key");
  CHECK_FALSE(llm.tts("hi", "alloy", "").has_value());
}

TEST_CASE("OpenAIClient constructs without a key") {
  OpenAIClient client(Env::fromMap({}));
  CHECK(client.name() == "openai");
}

// TODO(T01): live tests guarded by OPENAI_API_KEY (skip when unset).
TEST_CASE("TODO(T01): chatJson returns JSON matching the schema" * doctest::skip()) {}
TEST_CASE("TODO(T01): tts returns MP3 bytes and caches by text+voice" * doctest::skip()) {}
TEST_CASE("TODO(T01): realtimeSession mints an ephemeral key with expiry" * doctest::skip()) {}


TEST_CASE("FakeLlm handles nullable types written as a list") {
  FakeLlm llm;
  auto j = llm.chatJson("s", "u", {{"properties", {{"when", {{"type", {"integer", "null"}}}}}}});
  CHECK(j["when"] == 0);
}
