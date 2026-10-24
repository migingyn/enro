#include "model/TransportLearner.hpp"

namespace enro {

std::string TransportLearner::distanceBucket(int distance_m) {
  if (distance_m < 2000) return "<2km";
  if (distance_m < 5000) return "2-5km";
  if (distance_m < 15000) return "5-15km";
  return ">15km";
}

void TransportLearner::record(const std::string& destination_type, int distance_m, const std::string& mode) {
  if (mode.empty()) return;
  counts_[destination_type][distanceBucket(distance_m)][mode]++;
}

std::string TransportLearner::mostFrequent(const std::map<std::string, int>& counts, const std::string& preferred) {
  std::string best;
  int best_n = 0;
  for (auto& [mode, n] : counts) {
    if (n > best_n || (n == best_n && mode == preferred)) best = mode, best_n = n;
  }
  return best;
}

std::string TransportLearner::predict(const std::string& destination_type, int distance_m,
                                      const std::string& preferred) const {
  std::string fallback = preferred.empty() ? "walk" : preferred;
  auto type = counts_.find(destination_type);
  if (type == counts_.end()) return fallback;
  if (auto bucket = type->second.find(distanceBucket(distance_m)); bucket != type->second.end()) {
    std::string m = mostFrequent(bucket->second, preferred);
    if (!m.empty()) return m;
  }
  // No trips in this distance bucket yet: use the type's overall habit.
  std::map<std::string, int> all;
  for (auto& [bucket, modes] : type->second)
    for (auto& [mode, n] : modes) all[mode] += n;
  std::string m = mostFrequent(all, preferred);
  return m.empty() ? fallback : m;
}

std::vector<TransportCount> TransportLearner::counts() const {
  std::vector<TransportCount> out;
  for (auto& [type, buckets] : counts_)
    for (auto& [bucket, modes] : buckets) out.push_back({type, bucket, modes, mostFrequent(modes, "")});
  return out;
}

}  // namespace enro
