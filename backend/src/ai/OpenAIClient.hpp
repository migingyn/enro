// OpenAI client: chat with JSON output, TTS with tone instructions, transcription,
// and ephemeral Realtime keys for browser onboarding (PRD 8). Selected by LLM_PROVIDER=openai.
// Owner: T01. Model names come from env (OPENAI_CHAT_MODEL, OPENAI_TTS_MODEL, ...).
#pragma once

#include "ai/LlmClient.hpp"
#include "ai/SpeechClient.hpp"
#include "config/Env.hpp"

namespace enro {

class OpenAIClient : public LlmClient, public SpeechClient {
 public:
  explicit OpenAIClient(const Env& env);

  std::string name() const override { return "openai"; }

  nlohmann::json chatJson(const std::string& system, const std::string& user, const nlohmann::json& schema) override;
  std::string chat(const std::string& system, const std::vector<ChatMessage>& messages) override;

  std::optional<std::string> tts(const std::string& text, const std::string& voice,
                                 const std::string& instructions) override;
  std::string transcribe(const std::string& audio_bytes, const std::string& mime_type) override;
  RealtimeSession realtimeSession(const std::string& instructions) override;

 private:
  std::string api_key_;
  std::string chat_model_, tts_model_, realtime_model_, transcribe_model_;
};

}  // namespace enro
