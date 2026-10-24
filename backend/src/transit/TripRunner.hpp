// Plays a trip on a background thread at `speed`x real time, turning each position into a
// TripTick (Commentator) and broadcasting it as a `trip_tick` dashboard event. Owner: T15.
#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace enro {

struct AppContext;

struct TripRunInfo {
  bool ok = false;
  std::string trip;
  double speed = 1;
  int ticks = 0;
  double duration_s = 0;  // wall-clock length of the replay at this speed
};

class TripRunner {
 public:
  explicit TripRunner(AppContext& ctx) : ctx_(ctx) {}
  ~TripRunner() { stop(); }

  // Stops any running replay, then starts this one. Returns immediately.
  TripRunInfo start(const std::string& trip_id, double speed);
  void stop();
  bool running() const { return running_; }

 private:
  AppContext& ctx_;
  std::thread thread_;
  std::atomic<bool> running_{false}, stop_{false};
  std::mutex mu_;
};

}  // namespace enro
