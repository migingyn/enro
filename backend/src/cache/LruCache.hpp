// Generic LRU cache (PRD 4 scalability). Used for parsed emails (by message id), routes
// (by mode+from+to), and TTS audio (by text+voice). Owner: T05.
#pragma once

#include <cstddef>
#include <mutex>
#include <optional>

namespace enro {

template <typename K, typename V>
class LruCache {
 public:
  explicit LruCache(size_t capacity) : capacity_(capacity) {}

  std::optional<V> get(const K& key) {
    // STUB
    // TODO(T05): list + unordered_map, move hit to front, count hits/misses.
    (void)key;
    std::lock_guard<std::mutex> lock(mu_);
    ++misses_;
    return std::nullopt;
  }

  void put(const K& key, V value) {
    // STUB
    // TODO(T05): insert at front, evict the least recently used entry when size() > capacity_.
    (void)key;
    (void)value;
  }

  size_t size() const {
    // STUB
    // TODO(T05): entry count.
    return 0;
  }

  size_t capacity() const { return capacity_; }

  // hits / (hits + misses), 0 when unused. Log it so the dashboard and SCALE.md can cite it.
  double hitRate() const {
    std::lock_guard<std::mutex> lock(mu_);
    auto total = hits_ + misses_;
    return total == 0 ? 0.0 : double(hits_) / double(total);
  }

 private:
  size_t capacity_;
  mutable std::mutex mu_;
  size_t hits_ = 0, misses_ = 0;
};

}  // namespace enro
