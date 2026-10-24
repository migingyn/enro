// T05: LruCache + AudioCache + CachedRouter.
#include <doctest/doctest.h>

#include <filesystem>

#include "audio/AudioCache.hpp"
#include "cache/CachedRouter.hpp"
#include "cache/LruCache.hpp"

using namespace enro;

TEST_CASE("LruCache placeholder: miss on empty cache") {
  LruCache<std::string, int> c(2);
  CHECK_FALSE(c.get("a").has_value());
  CHECK(c.capacity() == 2);
  CHECK(AudioCache::keyFor("hi", "alloy", "") == AudioCache::keyFor("hi", "alloy", ""));
}

TEST_CASE("put then get hits") {
  LruCache<std::string, int> c(2);
  c.put("a", 1);
  REQUIRE(c.get("a").has_value());
  CHECK(*c.get("a") == 1);
  c.put("a", 7);
  CHECK(*c.get("a") == 7);
  CHECK(c.size() == 1);
}

TEST_CASE("evicts least recently used past capacity") {
  LruCache<std::string, int> c(2);
  c.put("a", 1);
  c.put("b", 2);
  c.put("c", 3);
  CHECK(c.size() == 2);
  CHECK_FALSE(c.get("a").has_value());
  CHECK(c.get("b").has_value());
  CHECK(c.get("c").has_value());
}

TEST_CASE("get refreshes recency") {
  LruCache<std::string, int> c(2);
  c.put("a", 1);
  c.put("b", 2);
  c.get("a");
  c.put("c", 3);
  CHECK(c.get("a").has_value());
  CHECK_FALSE(c.get("b").has_value());
}

TEST_CASE("hitRate counts hits and misses") {
  LruCache<int, int> c(4);
  CHECK(c.hitRate() == 0.0);
  c.put(1, 1);
  c.get(1);
  c.get(1);
  c.get(1);
  c.get(2);
  CHECK(c.hitRate() == doctest::Approx(0.75));
}

TEST_CASE("AudioCache get after put returns the same bytes, from memory and from disk") {
  auto dir = (std::filesystem::temp_directory_path() / "enro_audio_test").string();
  std::filesystem::remove_all(dir);
  std::string id = AudioCache::keyFor("Morning Sam", "alloy", "calm");
  {
    AudioCache a(dir);
    a.put(id, std::string("ID3\0mp3", 7));
    REQUIRE(a.get(id).has_value());
    CHECK(*a.get(id) == std::string("ID3\0mp3", 7));
  }
  AudioCache fresh(dir);  // a new process: disk only
  REQUIRE(fresh.get(id).has_value());
  CHECK(fresh.get(id)->size() == 7);
  CHECK_FALSE(fresh.get("../../etc/passwd").has_value());
  CHECK_FALSE(fresh.get("a0000000000000000").has_value());
  std::filesystem::remove_all(dir);
}

TEST_CASE("keyFor is stable FNV-1a and matches scripts/prerender.sh") {
  // FNV-1a 64 of "||" (empty voice, instructions, text).
  CHECK(AudioCache::keyFor("", "", "") == "a08e34c07b581a8a5");
  CHECK(AudioCache::keyFor("hi", "alloy", "").size() == 17);
  CHECK(AudioCache::keyFor("hi", "alloy", "") != AudioCache::keyFor("hi", "echo", ""));
}

TEST_CASE("CachedRouter serves repeat ETAs from cache") {
  FallbackRouter inner;
  CachedRouter r(inner, 8);
  Place a{"Center Hall", 32.8776, -117.2373}, b{"Geisel", 32.8811, -117.2376};
  auto first = r.eta("walk", a, b);
  auto second = r.eta("walk", a, b);
  CHECK(first.minutes == second.minutes);
  CHECK(r.hitRate() == doctest::Approx(0.5));
  CHECK(r.name() == "fallback+cache");
}
