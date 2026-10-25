#include "ai/AnthropicClient.hpp"

#include <algorithm>

#include <cpr/cpr.h>

#include "util/Log.hpp"

namespace enro {

namespace {

const char* kUrl = "https://api.anthropic.com/v1/messages";
const char* kVersion = "2026-10-21";
// fallbacks:"default" re-runs a refused request on Anthropic's recommended model server-side.
const char* kFallbackBeta = "server-side-fallback-2026-07-01";

// Every call sits on a live phone call or a plan update, so keep thinking short.
const char* kEffort = "low";
// Thinking tokens count against max_tokens, so leave room beyond the visible reply.
constexpr int kMaxTokens = 4000;

std::string replyText(const nlohmann::json& res) {
  std::string text;
  if (!res.is_object()) return text;
  for (auto& block : res.value("content", nlohmann::json::array()))
    if (block.value("type", "") == "text") text += block.value("text", "");
  return text;
}

}  // namespace

AnthropicClient::AnthropicClient(const Env& env)
    : api_key_(env.get("ANTHROPIC_API_KEY")), model_(env.get("ANTHROPIC_MODEL", "claude-opus-5-5")) {
  if (api_key_.empty()) LOG_WARN("anthropic", "LLM_PROVIDER=anthropic but ANTHROPIC_API_KEY is empty");
}

nlohmann::json AnthropicClient::strictSchema(const nlohmann::json& schema) {
  if (schema.is_array()) {
    nlohmann::json out = nlohmann::json::array();
    for (auto& s : schema) out.push_back(strictSchema(s));
    return out;
  }
  if (!schema.is_object()) return schema;
  static const std::vector<std::string> kUnsupported = {"minimum",   "maximum",   "exclusiveMinimum",
                                                        "exclusiveMaximum", "multipleOf", "minLength",
                                                        "maxLength", "minItems",  "maxItems"};
  nlohmann::json out = nlohmann::json::object();
  for (auto& [key, value] : schema.items()) {
    if (std::find(kUnsupported.begin(), kUnsupported.end(), key) != kUnsupported.end()) continue;
    if (key == "properties" && value.is_object()) {
      out[key] = nlohmann::json::object();
      for (auto& [prop, sub] : value.items()) out[key][prop] = strictSchema(sub);
    } else if (key == "items" || key == "anyOf" || key == "allOf") {
      out[key] = strictSchema(value);
    } else {
      out[key] = value;
    }
  }
  bool is_object = out.value("type", nlohmann::json()) == "object" || out.contains("properties");
  if (is_object) out["additionalProperties"] = false;
  return out;
}

nlohmann::json AnthropicClient::chatBody(const std::string& system, const std::vector<ChatMessage>& messages) const {
  nlohmann::json msgs = nlohmann::json::array();
  for (auto& m : messages) msgs.push_back({{"role", m.role}, {"content", m.content}});
  return {{"model", model_},
          {"max_tokens", kMaxTokens},
          {"system", system},
          {"messages", msgs},
          {"output_config", {{"effort", kEffort}}},
          {"fallbacks", "default"}};
}

nlohmann::json AnthropicClient::chatJsonBody(const std::string& system, const std::string& user,
                                             const nlohmann::json& schema) const {
  auto body = chatBody(system, {{"user", user}});
  body["output_config"]["format"] = {{"type", "json_schema"}, {"schema", strictSchema(schema)}};
  return body;
}

nlohmann::json AnthropicClient::post(const nlohmann::json& body) const {
  if (api_key_.empty()) return {};
  auto r = cpr::Post(cpr::Url{kUrl},
                     cpr::Header{{"x-api-key", api_key_},
                                 {"anthropic-version", kVersion},
                                 {"anthropic-beta", kFallbackBeta},
                                 {"content-type", "application/json"}},
                     cpr::Body{body.dump()}, cpr::Timeout{30000});
  if (r.status_code != 200) {
    LOG_ERROR("anthropic", "HTTP " << r.status_code << ": " << (r.error.message.empty() ? r.text : r.error.message));
    return {};
  }
  auto res = nlohmann::json::parse(r.text, nullptr, false);
  if (res.is_discarded()) {
    LOG_ERROR("anthropic", "response is not JSON");
    return {};
  }
  std::string stop = res.value("stop_reason", "");
  if (stop == "refusal") {
    LOG_WARN("anthropic", "request refused");
    return {};
  }
  if (stop == "max_tokens") LOG_WARN("anthropic", "reply hit max_tokens and may be cut off");
  return res;
}

std::string AnthropicClient::chat(const std::string& system, const std::vector<ChatMessage>& messages) {
  return replyText(post(chatBody(system, messages)));
}

nlohmann::json AnthropicClient::chatJson(const std::string& system, const std::string& user,
                                         const nlohmann::json& schema) {
  auto text = replyText(post(chatJsonBody(system, user, schema)));
  auto j = nlohmann::json::parse(text, nullptr, false);
  if (j.is_discarded() || !j.is_object()) {
    if (!text.empty()) LOG_WARN("anthropic", "chatJson reply is not a JSON object");
    return nlohmann::json::object();
  }
  return j;
}

}  // namespace enro
