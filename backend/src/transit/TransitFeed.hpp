// Vehicle positions for one trip. ReplayFeed plays a recorded trip from backend/seed/trips/
// (default); GtfsRtFeed reads a live GTFS-realtime feed (P1, needs protobuf). Owner: T15.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "config/Env.hpp"

namespace enro {

struct VehiclePosition {
  int seq = 0;
  long long ts = 0;  // recorded time
  double lat = 0, lng = 0;
  std::string next_stop;
  bool on_route = true;
};

struct PlannedStop {
  std::string name;
  double lat = 0, lng = 0;
};

struct TripInfo {
  std::string trip_id, route, destination;
  long long scheduled_arrival_ts = 0;
  long long start_ts = 0;           // first recorded position; with the stops it gives the planned pace
  std::vector<PlannedStop> stops;   // planned route, origin first, destination last
};

class TransitFeed {
 public:
  virtual ~TransitFeed() = default;
  virtual std::string name() const = 0;
  // Loads a trip. For replay this is a file name in backend/seed/trips without .json.
  virtual bool open(const std::string& trip_id) = 0;
  virtual TripInfo info() const = 0;
  // Next position, or nullopt when the trip is over.
  virtual std::optional<VehiclePosition> next() = 0;
  // Number of positions if known (replay), 0 for live feeds.
  virtual size_t size() const { return 0; }
};

class ReplayFeed : public TransitFeed {
 public:
  explicit ReplayFeed(std::string seed_dir);
  std::string name() const override { return "replay"; }
  bool open(const std::string& trip_id) override;
  TripInfo info() const override { return info_; }
  std::optional<VehiclePosition> next() override;
  size_t size() const override { return points_.size(); }

 private:
  std::string seed_dir_;
  TripInfo info_;
  std::vector<VehiclePosition> points_;
  size_t cursor_ = 0;
};

class GtfsRtFeed : public TransitFeed {
 public:
  explicit GtfsRtFeed(const Env& env);
  std::string name() const override { return "gtfs-rt"; }
  bool open(const std::string& trip_id) override;
  TripInfo info() const override { return info_; }
  std::optional<VehiclePosition> next() override;

 private:
  std::string vehicle_url_, trip_updates_url_;
  TripInfo info_;
};

}  // namespace enro
