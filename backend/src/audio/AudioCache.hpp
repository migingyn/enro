// TTS audio cache served at GET /audio/:id.mp3. Twilio <Play>s these URLs. Keyed by
// hash(text + voice + instructions) so repeated lines cost nothing (PRD 4). Owner: T05.
#pragma once

#include <optional>
#include <string>

namespace enro {

class AudioCache {
 public:
  explicit AudioCache(std::string dir) : dir_(std::move(dir)) {}

  static std::string keyFor(const std::string& text, const std::string& voice, const std::string& instructions);
  // MP3 bytes for an id, or nullopt.
  std::optional<std::string> get(const std::string& id);
  // Stores bytes under id (memory LRU + disk under dir_). Returns the id.
  std::string put(const std::string& id, const std::string& mp3);

 private:
  std::string dir_;
};

}  // namespace enro
