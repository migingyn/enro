// Generic LRU cache (PRD 4 scalability). Used for parsed emails (by message id), routes
// (by mode+from+to), and TTS audio (by text+voice). Owner: T05.
#pragma once

#include <cstddef>
#include <list>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <utility>

namespace enro {

template <typename K, typename V>
class LruCache {
 public:
  explicit LruCache(size_t capacity) : capacity_(capacity) {}

  std::optional<V> get(const K& key) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = index_.find(key);
    if (it == index_.end()) {
      ++misses_;
      return std::nullopt;
    }
    ++hits_;
    order_.splice(order_.begin(), order_, it->second);  // most recent at the front
    return it->second->second;
  }

  void put(const K& key, V value) {
    std::lock_guard<std::mutex> lock(mu_);
    if (capacity_ == 0) return;
    if (auto it = index_.find(key); it != index_.end()) {
      it->second->second = std::move(value);
      order_.splice(order_.begin(), order_, it->second);
      return;
    }
    order_.emplace_front(key, std::move(value));
    index_[key] = order_.begin();
    if (order_.size() > capacity_) {
      index_.erase(order_.back().first);
      order_.pop_back();
    }
  }

  size_t size() const {
    std::lock_guard<std::mutex> lock(mu_);
    return order_.size();
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
  std::list<std::pair<K, V>> order_;
  std::unordered_map<K, typename std::list<std::pair<K, V>>::iterator> index_;
  size_t hits_ = 0, misses_ = 0;
};

}  // namespace enro
