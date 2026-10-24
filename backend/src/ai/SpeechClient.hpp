// Voice interface: TTS, transcription, Realtime onboarding key. Always OpenAI when keys exist,
// even with LLM_PROVIDER=anthropic, because the Claude API has no audio output (PRD 8.1).
#pragma once

#include <optional>
#include <string>

namespace enro {

struct RealtimeSession {
  std::string client_secret;  // ephemeral key for the browser
  long long expires_at = 0;
  std::string model, voice;
};

class SpeechClient {
 public:
  virtual ~SpeechClient() = default;
  virtual std::string name() const = 0;

  // MP3 bytes, or nullopt when there is no TTS (the sim call screen then uses browser speech).
  virtual std::optional<std::string> tts(const std::string& text, const std::string& voice,
                                         const std::string& instructions) = 0;
  virtual std::string transcribe(const std::string& audio_bytes, const std::string& mime_type) = 0;
  virtual RealtimeSession realtimeSession(const std::string& instructions) = 0;
};

}  // namespace enro
