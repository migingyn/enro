// T01: AnthropicClient (Claude API alternate, PRD 8.1).
#include <doctest/doctest.h>

#include <cstdlib>

#include "ai/AnthropicClient.hpp"

using namespace enro;

TEST_CASE("AnthropicClient constructs without a key and returns empty results") {
  AnthropicClient client(Env::fromMap({}));
  CHECK(client.name() == "anthropic");
  CHECK(client.chat("sys", {{"user", "hi"}}) == "");
  CHECK(client.chatJson("sys", "hi", {{"type", "object"}}) == nlohmann::json::object());
}

TEST_CASE("strictSchema closes every object and drops unsupported constraints") {
  nlohmann::json schema = {
      {"type", "object"},
      {"properties",
       {{"minutes", {{"type", "integer"}, {"minimum", 0}, {"maximum", 60}}},
        {"stops", {{"type", "array"}, {"maxItems", 3}, {"items", {{"type", "object"}, {"properties", {{"name", {{"type", "string"}, {"maxLength", 20}}}}}}}}}}},
      {"required", {"minutes"}}};
  auto s = AnthropicClient::strictSchema(schema);
  CHECK(s["additionalProperties"] == false);
  CHECK(s["required"] == nlohmann::json{"minutes"});
  CHECK_FALSE(s["properties"]["minutes"].contains("minimum"));
  CHECK_FALSE(s["properties"]["minutes"].contains("maximum"));
  CHECK_FALSE(s["properties"]["stops"].contains("maxItems"));
  CHECK(s["properties"]["stops"]["items"]["additionalProperties"] == false);
  CHECK_FALSE(s["properties"]["stops"]["items"]["properties"]["name"].contains("maxLength"));
  // Nullable types written as a list pass through untouched.
  auto n = AnthropicClient::strictSchema({{"type", {"integer", "null"}}});
  CHECK(n["type"] == nlohmann::json{"integer", "null"});
}

TEST_CASE("request bodies use the configured model, low effort, fallbacks, and structured output") {
  AnthropicClient client(Env::fromMap({{"ANTHROPIC_MODEL", "claude-sonnet-5-5"}}));
  auto chat = client.chatBody("be brief", {{"user", "hi"}, {"assistant", "hey"}, {"user", "five more minutes"}});
  CHECK(chat["model"] == "claude-sonnet-5-5");
  CHECK(chat["system"] == "be brief");
  CHECK(chat["messages"].size() == 3);
  CHECK(chat["messages"][2]["content"] == "five more minutes");
  CHECK(chat["output_config"]["effort"] == "low");
  CHECK(chat["fallbacks"] == "default");
  CHECK_FALSE(chat.contains("tool_choice"));  // forced tool use is a 400 on current models

  auto js = client.chatJsonBody("classify", "{}", {{"type", "object"}, {"properties", {{"label", {{"type", "string"}}}}}});
  CHECK(js["output_config"]["format"]["type"] == "json_schema");
  CHECK(js["output_config"]["format"]["schema"]["additionalProperties"] == false);
  CHECK(js["messages"][0]["role"] == "user");

  AnthropicClient fallback_model(Env::fromMap({}));
  CHECK(fallback_model.chatBody("s", {})["model"] == "claude-opus-5-5");
}

// Live check. Runs only when ANTHROPIC_API_KEY is in the real environment (make test clears .env).
TEST_CASE("live: chatJson returns JSON matching the schema") {
  const char* key = std::getenv("ANTHROPIC_API_KEY");
  if (!key || !*key) return;
  AnthropicClient client(Env::fromMap({{"ANTHROPIC_API_KEY", key}}));
  nlohmann::json schema = {{"type", "object"},
                           {"properties",
                            {{"label", {{"type", "string"}, {"enum", {"SNOOZE_REQUEST", "AWAKE", "UNCLEAR"}}}},
                             {"minutes", {{"type", "integer"}}}}},
                           {"required", {"label", "minutes"}}};
  auto out = client.chatJson("Classify what a sleepy person said on a wake-up call.", "ugh give me ten more minutes",
                             schema);
  CHECK(out.value("label", "") == "SNOOZE_REQUEST");
  CHECK(out.value("minutes", 0) == 10);
  CHECK_FALSE(client.chat("Reply with one short sentence.", {{"user", "Say good morning."}}).empty());
}
