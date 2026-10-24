#include "transit/TripRunner.hpp"

#include <chrono>

#include "app/App.hpp"
#include "models/Json.hpp"
#include "util/Log.hpp"

namespace enro {

TripRunInfo TripRunner::start(const std::string& trip_id, double speed) {
  stop();
  std::lock_guard<std::mutex> lock(mu_);
  if (speed <= 0) speed = 1;

  auto feed = ctx_.makeTransitFeed();
  if (!feed->open(trip_id)) return {false, trip_id, speed, 0, 0};

  // Duration from the recording's own timestamps, for the response and the dashboard.
  // We re-open after peeking so the thread starts from the first position.
  std::vector<VehiclePosition> positions;
  while (auto p = feed->next()) positions.push_back(*p);
  double span = positions.size() > 1 ? double(positions.back().ts - positions.front().ts) : 0;
  TripRunInfo info{true, trip_id, speed, static_cast<int>(positions.size()), span / speed};
  TripInfo trip = feed->info();

  stop_ = false;
  running_ = true;
  thread_ = std::thread([this, positions = std::move(positions), trip, speed] {
    LOG_INFO("trip", "replaying " << trip.trip_id << " (" << positions.size() << " positions) at " << speed << "x");
    for (size_t i = 0; i < positions.size() && !stop_; ++i) {
      if (i > 0) {
        auto wait = std::chrono::duration<double>((positions[i].ts - positions[i - 1].ts) / speed);
        auto until = std::chrono::steady_clock::now() + wait;
        while (!stop_ && std::chrono::steady_clock::now() < until) std::this_thread::sleep_for(std::chrono::milliseconds(20));
        if (stop_) break;
      }
      TripTick tick = ctx_.commentator->tick(trip, positions[i]);
      ctx_.dashboard->broadcast("trip_tick", tick);
    }
    running_ = false;
    LOG_INFO("trip", "replay of " << trip.trip_id << " finished");
  });
  return info;
}

void TripRunner::stop() {
  stop_ = true;
  if (thread_.joinable()) thread_.join();
  running_ = false;
}

}  // namespace enro
