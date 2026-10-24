#include "config/Fixtures.hpp"
#include "transit/TransitFeed.hpp"
#include "util/Log.hpp"

namespace enro {

ReplayFeed::ReplayFeed(std::string seed_dir) : seed_dir_(std::move(seed_dir)) {}

bool ReplayFeed::open(const std::string& trip_id) {
  points_.clear();
  cursor_ = 0;
  auto j = Fixtures::readJsonFile(seed_dir_ + "/trips/" + trip_id + ".json");
  if (!j || !j->contains("points")) {
    LOG_WARN("transit", "trip file backend/seed/trips/" << trip_id << ".json missing or has no points");
    return false;
  }
  info_ = {j->value("trip_id", trip_id), j->value("route", ""), j->value("destination", ""),
           j->value("scheduled_arrival_ts", 0LL)};
  for (auto& p : (*j)["points"]) {
    points_.push_back({p.value("seq", 0), p.value("ts", 0LL), p.value("lat", 0.0), p.value("lng", 0.0),
                       p.value("next_stop", ""), p.value("on_route", true)});
  }
  LOG_INFO("transit", "loaded " << points_.size() << " positions for " << trip_id);
  return !points_.empty();
}

std::optional<VehiclePosition> ReplayFeed::next() {
  if (cursor_ >= points_.size()) return std::nullopt;
  return points_[cursor_++];
}

}  // namespace enro
