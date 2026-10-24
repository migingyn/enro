// Text generation interface (PRD 8.1). Selected by LLM_PROVIDER: fake | openai | anthropic.
// Covers line generation, intent classification, email extraction, onboarding profile extraction.
#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace enro {

struct ChatMessage {
  std::string role;  // "user" | "assistant"
  std::string content;
};

class LlmClient {
 public:
  virtual ~LlmClient() = default;
  virtual std::string name() const = 0;

  // Returns a JSON object that should match `schema` (a JSON Schema object).
  virtual nlohmann::json chatJson(const std::string& system, const std::string& user, const nlohmann::json& schema) = 0;
  // Returns the assistant's reply text.
  virtual std::string chat(const std::string& system, const std::vector<ChatMessage>& messages) = 0;
};

}  // namespace enro
