#include "transit/Commentator.hpp"

namespace enro {

Commentator::Commentator(Reflex& reflex, LineGenerator& lines) : reflex_(reflex), lines_(lines) {}

TripTick Commentator::tick(const TripInfo& trip, const VehiclePosition& pos) {
  // STUB
  // TODO(T15): compute eta_ts and delay_minutes from position + schedule, ask reflex_.transitSpeak,
  // generate a line with lines_ when verdict != IGNORE, and build a Reroute when the vehicle leaves the route.
  TripTick t;
  t.trip_id = trip.trip_id;
  t.seq = pos.seq;
  t.ts = pos.ts;
  t.lat = pos.lat;
  t.lng = pos.lng;
  t.route = trip.route;
  t.next_stop = pos.next_stop;
  t.eta_ts = trip.scheduled_arrival_ts;
  t.scheduled_arrival_ts = trip.scheduled_arrival_ts;
  t.delay_minutes = 0;
  t.on_route = pos.on_route;
  t.verdict = "IGNORE";
  t.line = "";
  return t;
}

}  // namespace enro
