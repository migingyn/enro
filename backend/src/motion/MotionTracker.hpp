// Phone-motion evidence from Sensor Logger's HTTP push (S7). Ported from the TypeScript
// prototype's core/motion.ts, same constants and the same tested behavior.
//
// Honest scope: this is a movement heuristic, not a wake or step classifier. It answers "has the
// phone been moving for a couple of seconds lately?" and nothing more. The 1.2 m/s² threshold
// has not been physically validated; tune it on the test phone.
//
// Inputs: only the calibrated "accelerometer" sensor (gravity already removed, m/s² on iOS and
// Android; never multiply by 9.80665). Gyroscope and raw accelerometer readings are ignored.
// Sample times are UTC epoch nanoseconds. Internally everything is epoch milliseconds (double).
//
// Thread-safe: Crow handlers run concurrently, so every public method takes the mutex.
#pragma once

#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace enro {

struct MotionIngestResult {
  int accepted = 0;
  int ignored = 0;
};

struct MotionSnapshot {
  std::string status = "unavailable";  // unavailable | stale | stationary | moving
  std::optional<double> last_sample_at, last_received_at;  // epoch ms
  int sample_count = 0, movement_samples = 0, movement_seconds = 0;
  std::optional<double> latest_magnitude, latency_ms;
  std::optional<std::string> device_id, source;  // source: "sensor-logger"
};
nlohmann::json toJson(const MotionSnapshot& s);

class MotionTracker {
 public:
  static constexpr double kFreshMs = 5'000;
  static constexpr double kFutureToleranceMs = 2'000;
  static constexpr double kRetainMs = 120'000;
  static constexpr size_t kMaxSamples = 12'000;
  static constexpr size_t kMaxBatch = 2'000;
  static constexpr size_t kMaxStreams = 64;
  // Initial demo threshold for gravity-removed acceleration magnitude. Not validated.
  static constexpr double kMovementThreshold = 1.2;

  // body: Sensor Logger push {messageId, sessionId, deviceId, payload:[{name, time, values:{x,y,z}}]}.
  // received_at_ms: server clock when the push arrived.
  MotionIngestResult ingest(const nlohmann::json& body, double received_at_ms);

  // since_ms restricts evidence to samples at or after it (a call's answered time).
  // Throws std::invalid_argument on a non-finite now, or a non-finite since other than -inf.
  MotionSnapshot snapshot(double now_ms, double since_ms = -std::numeric_limits<double>::infinity());

  // ---- per-call evidence (the wake flow) ----
  // Remembers when a call was answered. While it is tracked, every accepted push checks for movement
  // after that moment, so a burst of walking that ends before "I'm up" still counts.
  void markAnswered(const std::string& call_id, double answered_ms);
  std::optional<double> answeredAt(const std::string& call_id);
  bool movementObserved(const std::string& call_id);
  void forgetCall(const std::string& call_id);

 private:
  struct Sample {
    double at, received_at, magnitude;
    bool moving;
  };
  struct Stream {
    std::string key;
    long long message_id = 0;
    double last_sample_at = -std::numeric_limits<double>::infinity();
  };
  struct CallWatch {
    double answered_ms = 0;
    bool movement_observed = false;
  };
  static constexpr size_t kMaxCalls = 16;

  void prune(double now_ms);
  MotionSnapshot snapshotLocked(double now_ms, double since_ms);

  std::mutex mu_;
  std::vector<Sample> samples_;
  std::vector<Stream> streams_;  // least recently used first, at most kMaxStreams
  std::optional<std::string> active_stream_, device_id_;
  std::optional<Sample> latest_;
  std::vector<std::pair<std::string, CallWatch>> calls_;  // oldest first, at most kMaxCalls
};

}  // namespace enro
