// Live trip commentary (PRD 7.12). Each position becomes a TripTick: ETA, delay, the reflex
// verdict on whether to speak, and a commentator-style line when it should. Owner: T15.
#pragma once

#include "ai/LineGenerator.hpp"
#include "models/Models.hpp"
#include "reflex/Reflex.hpp"
#include "transit/TransitFeed.hpp"

namespace enro {

class Commentator {
 public:
  Commentator(Reflex& reflex, LineGenerator& lines);
  TripTick tick(const TripInfo& trip, const VehiclePosition& pos);

 private:
  Reflex& reflex_;
  LineGenerator& lines_;
};

}  // namespace enro
