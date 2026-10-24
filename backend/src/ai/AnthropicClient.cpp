// CLAUDE API ALTERNATE
// Switch with LLM_PROVIDER=anthropic and uncomment this file + its header (CMake globs it already).
// POST https://api.anthropic.com/v1/messages
// Headers: x-api-key: $ANTHROPIC_API_KEY, anthropic-version: <current>, content-type: application/json
// Body: { model: $ANTHROPIC_MODEL, max_tokens, system, messages: [{role, content}] }
// For chatJson, request JSON in the system prompt or use tool use with an input schema,
// then parse the first tool_use block's input. Response text lives in content[].text.
// Check current Anthropic docs for the version header, model names, and structured output options.
//
// #include "ai/AnthropicClient.hpp"
//
// #include <cpr/cpr.h>
//
// #include "util/Log.hpp"
//
// namespace enro {
//
// AnthropicClient::AnthropicClient(const Env& env)
//     : api_key_(env.get("ANTHROPIC_API_KEY")), model_(env.get("ANTHROPIC_MODEL")) {}
//
// static nlohmann::json post(const std::string& key, const nlohmann::json& body) {
//   auto r = cpr::Post(cpr::Url{"https://api.anthropic.com/v1/messages"},
//                      cpr::Header{{"x-api-key", key},
//                                  {"anthropic-version", "<current>"},
//                                  {"content-type", "application/json"}},
//                      cpr::Body{body.dump()}, cpr::Timeout{20000});
//   if (r.status_code != 200) {
//     LOG_ERROR("anthropic", "HTTP " << r.status_code << ": " << r.text);
//     return {};
//   }
//   return nlohmann::json::parse(r.text);
// }
//
// std::string AnthropicClient::chat(const std::string& system, const std::vector<ChatMessage>& messages) {
//   nlohmann::json msgs = nlohmann::json::array();
//   for (auto& m : messages) msgs.push_back({{"role", m.role}, {"content", m.content}});
//   auto res = post(api_key_, {{"model", model_}, {"max_tokens", 300}, {"system", system}, {"messages", msgs}});
//   std::string text;
//   for (auto& block : res.value("content", nlohmann::json::array()))
//     if (block.value("type", "") == "text") text += block.value("text", "");
//   return text;
// }
//
// nlohmann::json AnthropicClient::chatJson(const std::string& system, const std::string& user,
//                                          const nlohmann::json& schema) {
//   nlohmann::json tool = {{"name", "answer"}, {"description", "Return the structured answer"}, {"input_schema", schema}};
//   auto res = post(api_key_, {{"model", model_},
//                              {"max_tokens", 500},
//                              {"system", system},
//                              {"tools", {tool}},
//                              {"tool_choice", {{"type", "tool"}, {"name", "answer"}}},
//                              {"messages", {{{"role", "user"}, {"content", user}}}}});
//   for (auto& block : res.value("content", nlohmann::json::array()))
//     if (block.value("type", "") == "tool_use") return block["input"];
//   return nlohmann::json::object();
// }
//
// }  // namespace enro
