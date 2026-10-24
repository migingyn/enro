// Transport mode learning (PRD 7.9). Counts of mode by destination type and distance bucket.
// Predicted mode is the most frequent, with the stated preference as a tiebreaker. Owner: T13.
#pragma once

#include <string>
#include <vector>

#include "models/Models.hpp"

namespace enro {

class TransportLearner {
 public:
  static std::string distanceBucket(int distance_m);  // <2km | 2-5km | 5-15km | >15km

  void record(const std::string& destination_type, int distance_m, const std::string& mode);
  // preferred: the onboarding answer for this trip type, used when there is no data or a tie.
  std::string predict(const std::string& destination_type, int distance_m, const std::string& preferred) const;
  std::vector<TransportCount> counts() const;
};

}  // namespace enro
