#include "model/TransportLearner.hpp"

namespace enro {

std::string TransportLearner::distanceBucket(int distance_m) {
  if (distance_m < 2000) return "<2km";
  if (distance_m < 5000) return "2-5km";
  if (distance_m < 15000) return "5-15km";
  return ">15km";
}

void TransportLearner::record(const std::string&, int, const std::string&) {
  // STUB
  // TODO(T13): counts_[destination_type][distanceBucket(distance_m)][mode]++.
}

std::string TransportLearner::predict(const std::string&, int, const std::string& preferred) const {
  // STUB
  // TODO(T13): most frequent mode for (type, bucket); tiebreak with preferred; preferred when no data.
  return preferred.empty() ? "walk" : preferred;
}

std::vector<TransportCount> TransportLearner::counts() const {
  // STUB
  // TODO(T13): one TransportCount per (type, bucket) with its predicted mode.
  return {};
}

}  // namespace enro
