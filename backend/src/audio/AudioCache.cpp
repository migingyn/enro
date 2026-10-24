#include "audio/AudioCache.hpp"

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "util/Log.hpp"

namespace enro {

std::string AudioCache::keyFor(const std::string& text, const std::string& voice, const std::string& instructions) {
  std::uint64_t h = 0xcbf29ce484222325ULL;
  for (unsigned char c : voice + "|" + instructions + "|" + text) {
    h ^= c;
    h *= 0x100000001b3ULL;
  }
  char buf[20];
  std::snprintf(buf, sizeof buf, "a%016llx", static_cast<unsigned long long>(h));
  return buf;
}

// Ids come from URLs (/audio/:id.mp3), so only allow the characters keyFor produces.
bool AudioCache::safeId(const std::string& id) {
  if (id.empty() || id.size() > 64) return false;
  for (char c : id)
    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') return false;
  return true;
}

std::optional<std::string> AudioCache::get(const std::string& id) {
  if (!safeId(id)) return std::nullopt;
  if (auto hit = mem_.get(id)) return hit;
  std::ifstream f(dir_ + "/" + id + ".mp3", std::ios::binary);
  if (!f) return std::nullopt;
  std::stringstream ss;
  ss << f.rdbuf();
  std::string bytes = ss.str();
  mem_.put(id, bytes);
  LOG_INFO("cache", "audio " << id << " loaded from disk, memory hit rate " << mem_.hitRate());
  return bytes;
}

std::string AudioCache::put(const std::string& id, const std::string& mp3) {
  if (!safeId(id)) return id;
  mem_.put(id, mp3);
  std::error_code ec;
  std::filesystem::create_directories(dir_, ec);
  std::ofstream f(dir_ + "/" + id + ".mp3", std::ios::binary | std::ios::trunc);
  if (f) f << mp3;
  else LOG_WARN("cache", "could not write " << dir_ << "/" << id << ".mp3");
  return id;
}

}  // namespace enro
