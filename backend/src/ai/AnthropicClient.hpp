// Claude text client (PRD 8.1). Selected by LLM_PROVIDER=anthropic.
// Covers text only. TTS, transcription and Realtime onboarding stay on OpenAI.
// Model comes from ANTHROPIC_MODEL (default claude-opus-5-5).
#pragma once

#include "ai/LlmClient.hpp"
#include "config/Env.hpp"

namespace enro {

class AnthropicClient : public LlmClient {
 public:
  explicit AnthropicClient(const Env& env);

  std::string name() const override { return "anthropic"; }

  nlohmann::json chatJson(const std::string& system, const std::string& user, const nlohmann::json& schema) override;
  std::string chat(const std::string& system, const std::vector<ChatMessage>& messages) override;

  // Request bodies, public so tests can check them without a key.
  nlohmann::json chatBody(const std::string& system, const std::vector<ChatMessage>& messages) const;
  nlohmann::json chatJsonBody(const std::string& system, const std::string& user, const nlohmann::json& schema) const;

  // Structured outputs need additionalProperties:false on every object and reject
  // numeric/length constraints. Callers write plain JSON Schema, so fix it up here.
  static nlohmann::json strictSchema(const nlohmann::json& schema);

 private:
  nlohmann::json post(const nlohmann::json& body) const;

  std::string api_key_;
  std::string model_;
};

}  // namespace enro
