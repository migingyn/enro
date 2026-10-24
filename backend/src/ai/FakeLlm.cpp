#include "ai/FakeLlm.hpp"

#include "util/Time.hpp"

namespace enro {

nlohmann::json FakeLlm::chatJson(const std::string&, const std::string&, const nlohmann::json& schema) {
  nlohmann::json out = {{"fake", true}};
  if (schema.contains("properties")) {
    for (auto& [key, prop] : schema["properties"].items()) {
      // "type" may be a string or a list such as ["integer", "null"]; use the first entry.
      std::string type = "string";
      if (prop.contains("type")) type = prop["type"].is_array() ? prop["type"][0].get<std::string>() : prop["type"].get<std::string>();
      if (type == "string") out[key] = "";
      else if (type == "integer" || type == "number") out[key] = 0;
      else if (type == "boolean") out[key] = false;
      else if (type == "array") out[key] = nlohmann::json::array();
      else out[key] = nlohmann::json::object();
    }
  }
  return out;
}

std::string FakeLlm::chat(const std::string&, const std::vector<ChatMessage>& messages) {
  if (messages.empty()) return "Hey. Time to get moving.";
  return "Got it: \"" + messages.back().content + "\". Let's keep moving.";
}

std::optional<std::string> FakeLlm::tts(const std::string&, const std::string&, const std::string&) {
  return std::nullopt;
}

std::string FakeLlm::transcribe(const std::string&, const std::string&) { return ""; }

RealtimeSession FakeLlm::realtimeSession(const std::string&) {
  return {"fake-ephemeral-key", time::nowTs() + 60, "", ""};
}

}  // namespace enro
