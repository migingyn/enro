// Load simulation (PRD 4 scalability demo beat): plan N synthetic users across T threads,
// report runtime, calls-per-minute histogram, and cost per user per month.
// Target: 100,000 users under 5 seconds. Owner: T16.
#pragma once

#include "models/Models.hpp"

namespace enro {

struct CostRates {  // USD, tune in docs/SCALE.md (T23)
  double llm_per_call = 0.0004;
  double tts_per_call = 0.0003;
  double telephony_per_min = 0.014;
  double reflex_per_decision = 0.00002;
};

class LoadSimulator {
 public:
  explicit LoadSimulator(CostRates rates = {}) : rates_(rates) {}
  SimResult run(int users, int threads);

 private:
  CostRates rates_;
};

}  // namespace enro
