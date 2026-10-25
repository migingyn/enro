#include "motion/MotionTracker.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>
#include <stdexcept>

namespace enro {

namespace {

bool identifier(const nlohmann::json& j, const char* key) {
  auto it = j.find(key);
  if (it == j.end() || !it->is_string()) return false;
  const auto& s = it->get_ref<const std::string&>();
  bool blank = std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c); });
  return !blank && s.size() <= 200;
}

std::optional<double> finite(const nlohmann::json& j) {
  if (!j.is_number()) return std::nullopt;
  double v = j.get<double>();
  if (!std::isfinite(v)) return std::nullopt;
  return v;
}

// Sensor Logger sends an increasing integer. Same bounds as JS Number.isSafeInteger, and not negative.
std::optional<long long> messageId(const nlohmann::json& body) {
  auto it = body.find("messageId");
  if (it == body.end() || !it->is_number()) return std::nullopt;
  constexpr double kMaxSafe = 9007199254740991.0;
  if (it->is_number_float()) {
    double v = it->get<double>();
    if (!std::isfinite(v) || v != std::floor(v) || v < 0 || v > kMaxSafe) return std::nullopt;
    return static_cast<long long>(v);
  }
  if (it->is_number_unsigned()) {
    auto v = it->get<unsigned long long>();
    if (v > static_cast<unsigned long long>(kMaxSafe)) return std::nullopt;
    return static_cast<long long>(v);
  }
  long long v = it->get<long long>();
  if (v < 0 || v > static_cast<long long>(kMaxSafe)) return std::nullopt;
  return v;
}

long long secondOf(double at_ms) { return static_cast<long long>(std::floor(at_ms / 1000)); }

}  // namespace

nlohmann::json toJson(const MotionSnapshot& s) {
  auto opt = [](const auto& v) { return v ? nlohmann::json(*v) : nlohmann::json(nullptr); };
  return {{"status", s.status},
          {"last_sample_at", opt(s.last_sample_at)},
          {"last_received_at", opt(s.last_received_at)},
          {"sample_count", s.sample_count},
          {"movement_samples", s.movement_samples},
          {"movement_seconds", s.movement_seconds},
          {"latest_magnitude", opt(s.latest_magnitude)},
          {"latency_ms", opt(s.latency_ms)},
          {"device_id", opt(s.device_id)},
          {"source", opt(s.source)}};
}

MotionIngestResult MotionTracker::ingest(const nlohmann::json& body, double received_at) {
  bool has_payload = body.is_object() && body.contains("payload") && body["payload"].is_array();
  int count = has_payload ? static_cast<int>(body["payload"].size()) : 1;
  auto msg = has_payload ? messageId(body) : std::nullopt;
  if (!std::isfinite(received_at) || !has_payload || body["payload"].size() > kMaxBatch ||
      !identifier(body, "deviceId") || !identifier(body, "sessionId") || !msg)
    return {0, count};

  std::lock_guard lock(mu_);
  const std::string device = body["deviceId"].get<std::string>();
  const std::string key = nlohmann::json::array({device, body["sessionId"]}).dump();

  // Replay guard per (device, session): message IDs must strictly increase.
  Stream stream{key, *msg};
  auto found = std::find_if(streams_.begin(), streams_.end(), [&](auto& s) { return s.key == key; });
  if (found != streams_.end()) {
    if (*msg <= found->message_id) return {0, count};
    stream.last_sample_at = found->last_sample_at;
    streams_.erase(found);
  }
  streams_.push_back(stream);
  if (streams_.size() > kMaxStreams) streams_.erase(streams_.begin());
  Stream& s = streams_.back();

  int accepted = 0;
  for (const auto& reading : body["payload"]) {
    if (!reading.is_object() || !reading.contains("name") || reading["name"] != "accelerometer") continue;
    auto time = reading.contains("time") ? finite(reading["time"]) : std::nullopt;
    if (!time || !reading.contains("values") || !reading["values"].is_object()) continue;
    const auto& values = reading["values"];
    double xyz[3];
    bool ok = true;
    for (int i = 0; i < 3 && ok; ++i) {
      const char* axis = i == 0 ? "x" : i == 1 ? "y" : "z";
      auto v = values.contains(axis) ? finite(values[axis]) : std::nullopt;
      ok = v && std::abs(*v) <= 200;
      if (ok) xyz[i] = *v;
    }
    if (!ok) continue;
    double at = *time / 1'000'000;  // epoch ns -> ms. Epoch s or ms land far in the past and are dropped.
    if (at < received_at - kFreshMs || at > received_at + kFutureToleranceMs || at <= s.last_sample_at ||
        (latest_ && at <= latest_->at))
      continue;
    // Never combine movement from different devices or recording sessions.
    if (active_stream_ != key) {
      samples_.clear();
      active_stream_ = key;
      device_id_ = device;
    }
    double magnitude = std::sqrt(xyz[0] * xyz[0] + xyz[1] * xyz[1] + xyz[2] * xyz[2]);
    Sample sample{at, received_at, magnitude, magnitude >= kMovementThreshold};
    s.last_sample_at = at;
    latest_ = sample;
    samples_.push_back(sample);
    ++accepted;
  }
  prune(received_at);

  if (accepted > 0) {
    for (auto& [id, watch] : calls_)
      if (!watch.movement_observed) watch.movement_observed = snapshotLocked(received_at, watch.answered_ms).status == "moving";
  }
  return {accepted, count - accepted};
}

MotionSnapshot MotionTracker::snapshot(double now, double since) {
  if (!std::isfinite(now) || (!std::isfinite(since) && since != -std::numeric_limits<double>::infinity()))
    throw std::invalid_argument("Invalid motion snapshot time.");
  std::lock_guard lock(mu_);
  return snapshotLocked(now, since);
}

MotionSnapshot MotionTracker::snapshotLocked(double now, double since) {
  prune(now);
  std::optional<Sample> latest = latest_ && latest_->at >= since ? latest_ : std::nullopt;

  MotionSnapshot out;
  std::set<long long> moving_seconds, recent_seconds;
  std::optional<double> first_recent, last_recent;
  int recent_count = 0;
  for (const auto& sample : samples_) {
    if (sample.at < since) continue;
    ++out.sample_count;
    if (!sample.moving) continue;
    ++out.movement_samples;
    moving_seconds.insert(secondOf(sample.at));
    if (sample.at >= now - kFreshMs && sample.at <= now + kFutureToleranceMs) {
      recent_seconds.insert(secondOf(sample.at));
      if (!first_recent) first_recent = sample.at;
      last_recent = sample.at;
      ++recent_count;
    }
  }
  out.movement_seconds = static_cast<int>(moving_seconds.size());
  // A short shake crossing a wall-clock second boundary is still insufficient.
  bool spans_second = recent_count >= 2 && *last_recent - *first_recent >= 1'000;
  bool fresh = latest && now - latest->at <= kFreshMs && latest->at - now <= kFutureToleranceMs &&
               now - latest->received_at <= kFreshMs;
  out.status = !latest ? "unavailable"
               : !fresh ? "stale"
               : recent_seconds.size() >= 2 && spans_second ? "moving"
                                                            : "stationary";
  if (latest) {
    out.last_sample_at = latest->at;
    out.last_received_at = latest->received_at;
    out.latest_magnitude = latest->magnitude;
    out.latency_ms = std::max(0.0, latest->received_at - latest->at);
    out.device_id = device_id_;
    out.source = "sensor-logger";
  }
  return out;
}

void MotionTracker::prune(double now) {
  double cutoff = now - kRetainMs;
  auto first = std::find_if(samples_.begin(), samples_.end(), [&](auto& s) { return s.at >= cutoff; });
  size_t drop = static_cast<size_t>(first - samples_.begin());
  if (samples_.size() > kMaxSamples) drop = std::max(drop, samples_.size() - kMaxSamples);
  if (drop > 0) samples_.erase(samples_.begin(), samples_.begin() + static_cast<long>(drop));
}

void MotionTracker::markAnswered(const std::string& call_id, double answered_ms) {
  std::lock_guard lock(mu_);
  auto it = std::find_if(calls_.begin(), calls_.end(), [&](auto& c) { return c.first == call_id; });
  if (it != calls_.end()) return;  // keep the first answer time
  calls_.push_back({call_id, {answered_ms, false}});
  if (calls_.size() > kMaxCalls) calls_.erase(calls_.begin());
}

std::optional<double> MotionTracker::answeredAt(const std::string& call_id) {
  std::lock_guard lock(mu_);
  for (auto& [id, watch] : calls_)
    if (id == call_id) return watch.answered_ms;
  return std::nullopt;
}

bool MotionTracker::movementObserved(const std::string& call_id) {
  std::lock_guard lock(mu_);
  for (auto& [id, watch] : calls_)
    if (id == call_id) return watch.movement_observed;
  return false;
}

void MotionTracker::forgetCall(const std::string& call_id) {
  std::lock_guard lock(mu_);
  std::erase_if(calls_, [&](auto& c) { return c.first == call_id; });
}

}  // namespace enro
