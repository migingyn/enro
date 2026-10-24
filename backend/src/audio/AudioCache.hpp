// TTS audio cache served at GET /audio/:id.mp3. Twilio <Play>s these URLs. Keyed by
// hash(text + voice + instructions) so repeated lines cost nothing (PRD 4). Owner: T05.
// Memory LRU in front of dir_/<id>.mp3 on disk; scripts/prerender.sh fills the disk side with the same keys.
#pragma once

#include <optional>
#include <string>

#include "cache/LruCache.hpp"

namespace enro {

class AudioCache {
 public:
  explicit AudioCache(std::string dir, size_t memory_entries = 256) : dir_(std::move(dir)), mem_(memory_entries) {}

  // "a" + FNV-1a 64 of voice|instructions|text as 16 hex digits. Stable across builds; prerender.sh matches it.
  static std::string keyFor(const std::string& text, const std::string& voice, const std::string& instructions);
  // MP3 bytes for an id, or nullopt.
  std::optional<std::string> get(const std::string& id);
  // Stores bytes under id (memory LRU + disk under dir_). Returns the id.
  std::string put(const std::string& id, const std::string& mp3);
  double hitRate() const { return mem_.hitRate(); }

 private:
  static bool safeId(const std::string& id);
  std::string dir_;
  LruCache<std::string, std::string> mem_;
};

}  // namespace enro
