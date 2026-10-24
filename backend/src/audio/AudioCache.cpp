#include "audio/AudioCache.hpp"

#include <functional>

namespace enro {

std::string AudioCache::keyFor(const std::string& text, const std::string& voice, const std::string& instructions) {
  return "a" + std::to_string(std::hash<std::string>{}(voice + "|" + instructions + "|" + text));
}

std::optional<std::string> AudioCache::get(const std::string&) {
  // STUB
  // TODO(T05): LruCache<std::string, std::string> in memory, then dir_/<id>.mp3 on disk (scripts/prerender.sh fills it).
  return std::nullopt;
}

std::string AudioCache::put(const std::string& id, const std::string&) {
  // STUB
  // TODO(T05): write dir_/<id>.mp3 and insert into the LRU.
  return id;
}

}  // namespace enro
