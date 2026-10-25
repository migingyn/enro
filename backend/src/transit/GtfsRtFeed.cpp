// Live GTFS-realtime feed (P1). Needs protobuf: when CMake finds it, ENRO_HAVE_PROTOBUF is defined.
// Install notes are in the README. Without protobuf this feed always reports no positions.
#include "transit/TransitFeed.hpp"
#include "util/Log.hpp"

namespace enro {

GtfsRtFeed::GtfsRtFeed(const Env& env)
    : vehicle_url_(env.get("GTFS_RT_VEHICLE_URL")), trip_updates_url_(env.get("GTFS_RT_TRIP_UPDATES_URL")) {
#ifndef ENRO_HAVE_PROTOBUF
  LOG_WARN("transit", "GTFS-RT selected but built without protobuf; live feed disabled");
#endif
}

bool GtfsRtFeed::open(const std::string& trip_id) {
  // STUB: live GTFS-RT is P1. It needs protobuf, classes generated from gtfs-realtime.proto (not in
  // the repo yet) and network, so it can't be built or tested in fake mode. Replay is the demo path.
  // TODO(T15): fetch the first VehiclePositions + TripUpdates snapshot with cpr.
  info_.trip_id = trip_id;
  LOG_WARN("transit", "GTFS-RT feed not implemented yet; use TRANSIT_SOURCE=replay");
  return false;
}

std::optional<VehiclePosition> GtfsRtFeed::next() {
  // STUB
  // TODO(T15): poll vehicle_url_ every POLL_SECONDS with cpr, parse gtfs-realtime.proto FeedMessage,
  // find this trip's vehicle, return its position; on_route from shape distance.
  return std::nullopt;
}

}  // namespace enro
