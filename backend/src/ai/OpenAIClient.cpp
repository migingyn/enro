#include "ai/OpenAIClient.hpp"

#include "util/Log.hpp"
#include "util/Time.hpp"

namespace enro {

OpenAIClient::OpenAIClient(const Env& env)
    : api_key_(env.get("OPENAI_API_KEY")),
      chat_model_(env.get("OPENAI_CHAT_MODEL")),
      tts_model_(env.get("OPENAI_TTS_MODEL")),
      realtime_model_(env.get("OPENAI_REALTIME_MODEL")),
      transcribe_model_(env.get("OPENAI_TRANSCRIBE_MODEL")) {
  if (api_key_.empty()) LOG_WARN("openai", "LLM_PROVIDER=openai but OPENAI_API_KEY is empty");
}

nlohmann::json OpenAIClient::chatJson(const std::string&, const std::string&, const nlohmann::json&) {
  // STUB
  // TODO(T01): POST /v1/chat/completions (or /v1/responses) with JSON schema output, parse the JSON.
  LOG_WARN("openai", "chatJson not implemented yet (T01)");
  return {{"fake", true}};
}

std::string OpenAIClient::chat(const std::string&, const std::vector<ChatMessage>&) {
  // STUB
  // TODO(T01): POST chat with system + messages, return the assistant text.
  LOG_WARN("openai", "chat not implemented yet (T01)");
  return "Time to get moving.";
}

std::optional<std::string> OpenAIClient::tts(const std::string&, const std::string&, const std::string&) {
  // STUB
  // TODO(T01): POST /v1/audio/speech with model, voice, instructions; return MP3 bytes. Cache via AudioCache (T05).
  return std::nullopt;
}

std::string OpenAIClient::transcribe(const std::string&, const std::string&) {
  // STUB
  // TODO(T01): POST /v1/audio/transcriptions (multipart) and return the text.
  return "";
}

RealtimeSession OpenAIClient::realtimeSession(const std::string&) {
  // STUB
  // TODO(T01): mint an ephemeral Realtime client secret for the browser (WebRTC). Check current docs.
  return {"fake-ephemeral-key", time::nowTs() + 60, realtime_model_, ""};
}

}  // namespace enro
