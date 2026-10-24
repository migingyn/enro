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
  // STUB
  // TODO(T15): remember trip_id, fetch the first VehiclePositions + TripUpdates snapshot.
  info_.trip_id = trip_id;
  return false;
}

std::optional<VehiclePosition> GtfsRtFeed::next() {
  // STUB
  // TODO(T15): poll vehicle_url_ every POLL_SECONDS with cpr, parse gtfs-realtime.proto FeedMessage,
  // find this trip's vehicle, return its position; on_route from shape distance.
  return std::nullopt;
}

}  // namespace enro
