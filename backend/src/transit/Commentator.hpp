// Live trip commentary (PRD 7.12). Each position becomes a TripTick: ETA, delay, the reflex
// verdict on whether to speak, and a commentator-style line when it should. Owner: T15.
#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "ai/LineGenerator.hpp"
#include "models/Models.hpp"
#include "reflex/Reflex.hpp"
#include "transit/TransitFeed.hpp"

namespace enro {

class Commentator {
 public:
  Commentator(Reflex& reflex, LineGenerator& lines);
  TripTick tick(const TripInfo& trip, const VehiclePosition& pos);
  // Forget per-trip state (last stop, whether we already rerouted). Called before each replay.
  void reset();

  // The reflex verdict and inputs behind the last tick, for the Decision row.
  const ReflexVerdict& lastVerdict() const { return last_verdict_; }
  const nlohmann::json& lastInputs() const { return last_inputs_; }

  // ETA from the remaining distance along the planned stops at the planned average pace.
  // Without stops or a schedule the ETA is the scheduled arrival.
  static long long estimateEta(const TripInfo& trip, const VehiclePosition& pos);

 private:
  Reflex& reflex_;
  LineGenerator& lines_;
  std::string trip_id_, last_stop_;
  bool rerouted_ = false;  // one detour, one reroute: later off-route ticks stay quiet
  ReflexVerdict last_verdict_;
  nlohmann::json last_inputs_ = nlohmann::json::object();
};

}  // namespace enro
