// T05: LruCache + AudioCache.
#include <doctest/doctest.h>

#include "audio/AudioCache.hpp"
#include "cache/LruCache.hpp"

using namespace enro;

TEST_CASE("LruCache placeholder: miss on empty cache") {
  LruCache<std::string, int> c(2);
  CHECK_FALSE(c.get("a").has_value());
  CHECK(c.capacity() == 2);
  CHECK(AudioCache::keyFor("hi", "alloy", "") == AudioCache::keyFor("hi", "alloy", ""));
}

TEST_CASE("TODO(T05): put then get hits" * doctest::skip()) {}
TEST_CASE("TODO(T05): evicts least recently used past capacity" * doctest::skip()) {}
TEST_CASE("TODO(T05): get refreshes recency" * doctest::skip()) {}
TEST_CASE("TODO(T05): hitRate counts hits and misses" * doctest::skip()) {}
TEST_CASE("TODO(T05): AudioCache get after put returns the same bytes" * doctest::skip()) {}
