// Fake text + speech provider for LLM_PROVIDER=fake (the default). Deterministic, no network.
#pragma once

#include "ai/LlmClient.hpp"
#include "ai/SpeechClient.hpp"

namespace enro {

class FakeLlm : public LlmClient, public SpeechClient {
 public:
  std::string name() const override { return "fake"; }

  // Returns {"fake": true} plus a default value for each top-level property in the schema.
  nlohmann::json chatJson(const std::string& system, const std::string& user, const nlohmann::json& schema) override;
  std::string chat(const std::string& system, const std::vector<ChatMessage>& messages) override;

  std::optional<std::string> tts(const std::string& text, const std::string& voice,
                                 const std::string& instructions) override;
  std::string transcribe(const std::string& audio_bytes, const std::string& mime_type) override;
  RealtimeSession realtimeSession(const std::string& instructions) override;
};

}  // namespace enro
