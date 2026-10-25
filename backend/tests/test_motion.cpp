// S7: phone motion from Sensor Logger. Cases ported from the prototype's test/core-motion.test.ts.
#include <doctest/doctest.h>

#include <cmath>

#include "TestApp.hpp"
#include "motion/MotionTracker.hpp"
#include "motion/WakeEvidence.hpp"
#include "store/Seed.hpp"
#include "util/Time.hpp"

using namespace enro;
using json = nlohmann::json;

namespace {

const double kNow = 1792875610000.0;  // 2026-10-24T21:00:10Z in epoch ms

json sample(double at_ms, double x = 2, const std::string& name = "accelerometer") {
  return {{"name", name}, {"time", at_ms * 1'000'000}, {"values", {{"x", x}, {"y", 0}, {"z", 0}}}};
}

json batch(long long message_id, json payload, const std::string& device = "phone",
           const std::string& session = "recording") {
  return {{"messageId", message_id}, {"deviceId", device}, {"sessionId", session}, {"payload", std::move(payload)}};
}

void checkResult(MotionIngestResult r, int accepted, int ignored) {
  CHECK(r.accepted == accepted);
  CHECK(r.ignored == ignored);
}

}  // namespace

TEST_CASE("motion starts unavailable and accepts Sensor Logger nanosecond payloads") {
  MotionTracker m;
  CHECK(m.snapshot(kNow).status == "unavailable");
  checkResult(m.ingest(batch(0, {sample(kNow - 1500), sample(kNow - 200)}), kNow), 2, 0);
  auto s = m.snapshot(kNow);
  CHECK(s.status == "moving");
  CHECK(s.sample_count == 2);
  CHECK(s.movement_samples == 2);
  CHECK(s.movement_seconds == 2);
  CHECK(*s.last_sample_at == doctest::Approx(kNow - 200));
  CHECK(*s.latency_ms == doctest::Approx(200));
  CHECK(*s.device_id == "phone");
  CHECK(*s.source == "sensor-logger");
}

TEST_CASE("rotation and a brief shake across a second boundary are not sustained motion") {
  MotionTracker m;
  checkResult(m.ingest(batch(0, {sample(kNow - 3000, 100, "gyroscope"), sample(kNow - 2000, 0.05),
                                 sample(kNow - 1900, 0.02)}),
                       kNow),
              2, 1);
  CHECK(m.snapshot(kNow).status == "stationary");
  // Two big readings 2 ms apart land in two different wall-clock seconds, but span < 1 s.
  m.ingest(batch(1, {sample(kNow - 1001, 12), sample(kNow - 999, 12)}), kNow);
  CHECK(m.snapshot(kNow).movement_seconds == 2);
  CHECK(m.snapshot(kNow).status == "stationary");
}

TEST_CASE("motion status expires to stationary, then stale") {
  MotionTracker m;
  m.ingest(batch(0, {sample(kNow - 1500), sample(kNow - 200)}), kNow);
  CHECK(m.snapshot(kNow + 4000).status == "stationary");
  CHECK(m.snapshot(kNow + 4000).sample_count == 2);
  CHECK(m.snapshot(kNow + 5000).status == "stale");
  m.ingest(batch(1, {sample(kNow + 5500, 0)}), kNow + 5500);
  CHECK(m.snapshot(kNow + 5500).status == "stationary");
}

TEST_CASE("stale, future, malformed samples and epoch seconds or milliseconds are ignored") {
  MotionTracker m;
  json invalid = {sample(kNow - 5001),
                  sample(kNow + 2001),
                  sample(kNow, NAN),
                  sample(kNow, 201),
                  {{"name", "accelerometer"}, {"time", kNow}, {"values", {{"x", 2}, {"y", 0}, {"z", 0}}}},
                  {{"name", "accelerometer"}, {"time", kNow * 1'000'000}, {"values", {{"x", 1}}}},
                  nullptr};
  checkResult(m.ingest(batch(0, invalid), kNow), 0, static_cast<int>(invalid.size()));
  CHECK(m.snapshot(kNow).status == "unavailable");
}

TEST_CASE("replayed and lower message IDs, duplicate and out-of-order sample times are ignored") {
  MotionTracker m;
  json body = batch(5, {sample(kNow - 1500)});
  m.ingest(body, kNow);
  checkResult(m.ingest(body, kNow), 0, 1);
  checkResult(m.ingest(batch(4, {sample(kNow - 200)}), kNow), 0, 1);
  checkResult(m.ingest(batch(6, {sample(kNow - 1500), sample(kNow - 1600), sample(kNow - 200)}), kNow), 1, 2);
  CHECK(m.snapshot(kNow).sample_count == 2);
}

TEST_CASE("since (call answered) excludes evidence from before the call") {
  MotionTracker m;
  m.ingest(batch(0, {sample(kNow - 1500), sample(kNow - 200)}), kNow);
  CHECK(m.snapshot(kNow, kNow - 100).status == "unavailable");
  m.ingest(batch(1, {sample(kNow + 100)}), kNow + 100);
  CHECK(m.snapshot(kNow + 100, kNow - 100).status == "stationary");
  CHECK(m.snapshot(kNow + 100, kNow - 100).sample_count == 1);
  m.ingest(batch(2, {sample(kNow + 1200)}), kNow + 1200);
  CHECK(m.snapshot(kNow + 1200, kNow - 100).status == "moving");
}

TEST_CASE("readings from different devices or sessions never combine") {
  MotionTracker m;
  m.ingest(batch(0, {sample(kNow - 1500)}), kNow);
  m.ingest(batch(0, {sample(kNow - 200)}, "other-phone"), kNow);
  CHECK(m.snapshot(kNow).status == "stationary");
  CHECK(*m.snapshot(kNow).device_id == "other-phone");
  CHECK(m.snapshot(kNow).sample_count == 1);
  m.ingest(batch(0, {sample(kNow)}, "other-phone", "new-session"), kNow);
  CHECK(m.snapshot(kNow).sample_count == 1);
}

TEST_CASE("malformed and oversized batches add no evidence") {
  MotionTracker m;
  json no_session = batch(0, {sample(kNow)});
  no_session["sessionId"] = "";
  json bodies[] = {nullptr, json::object(), json::array(), batch(-1, {sample(kNow)}), no_session,
                   batch(0, {sample(kNow)}, "   ")};
  for (auto& body : bodies) CHECK(m.ingest(body, kNow).accepted == 0);
  json big = json::array();
  for (int i = 0; i < 2001; ++i) big.push_back(sample(kNow));
  checkResult(m.ingest(batch(0, big), kNow), 0, 2001);
  CHECK(m.snapshot(kNow).sample_count == 0);
}

TEST_CASE("retention is bounded and stale metadata outlives the history") {
  MotionTracker m;
  for (int i = 0; i < 8; ++i) {
    double at = kNow + i * 1000;
    json payload = json::array();
    for (int j = 0; j < 2000; ++j) payload.push_back(sample(at + j / 2.0, 0));
    m.ingest(batch(i, payload), at + 1000);
  }
  CHECK(m.snapshot(kNow + 9000).sample_count <= 12'000);
  CHECK(m.snapshot(kNow + 200'000).sample_count == 0);
  CHECK(m.snapshot(kNow + 200'000).status == "stale");
}

TEST_CASE("snapshot rejects non-finite times") {
  MotionTracker m;
  CHECK_THROWS(m.snapshot(NAN));
  CHECK_THROWS(m.snapshot(kNow, NAN));
}

TEST_CASE("call watch remembers movement seen after the answer") {
  MotionTracker m;
  m.markAnswered("c1", kNow);
  m.ingest(batch(0, {sample(kNow + 100), sample(kNow + 1300)}), kNow + 1300);
  CHECK(m.movementObserved("c1"));
  // Long after, the snapshot is stale, but the call already saw movement.
  CHECK(m.snapshot(kNow + 60'000, kNow).status == "stale");
  CHECK(m.movementObserved("c1"));
  m.forgetCall("c1");
  CHECK_FALSE(m.answeredAt("c1"));
}

// ---------- wake flow integration ----------

namespace {
std::optional<Decision> lastEvidence(AppContext& app) {
  for (auto& d : app.repos->recentDecisions())
    if (d.kind == "wake_evidence") return d;
  return std::nullopt;
}

void passAwakeCheck(AppContext& app, const std::string& id) {
  app.conversation->onEvent(id, "speech", "Okay okay, I'm up");
  auto done = app.conversation->onEvent(id, "speech", "CSE 110 at eight");
  REQUIRE(done);
  REQUIRE(done->end_call);
}
}  // namespace

TEST_CASE("'I'm up' writes a wake_evidence Decision: moving after the answer") {
  auto app = test::fakeApp();
  seedDemo(*app);
  auto id = app->wake_ladder->start("u_demo");
  app->conversation->onEvent(id, "answered", "");
  double t0 = static_cast<double>(time::nowMs()) + 1;
  CHECK(app->motion->ingest(batch(0, {sample(t0 + 100), sample(t0 + 1300)}), t0).accepted == 2);
  passAwakeCheck(*app, id);
  auto d = lastEvidence(*app);
  REQUIRE(d);
  CHECK(d->verdict == "moving");
  CHECK(d->source == "rules");
  CHECK(d->inputs["movement_observed"] == true);
  CHECK(d->inputs["call_id"] == id);
}

TEST_CASE("'I'm up' with only pre-answer movement has no evidence") {
  auto app = test::fakeApp();
  seedDemo(*app);
  auto id = app->wake_ladder->start("u_demo");
  double t0 = static_cast<double>(time::nowMs());
  app->motion->ingest(batch(0, {sample(t0 - 1500), sample(t0 - 200)}), t0);
  app->conversation->onEvent(id, "answered", "");
  passAwakeCheck(*app, id);
  auto d = lastEvidence(*app);
  REQUIRE(d);
  CHECK(d->verdict == "unavailable");
  CHECK(d->inputs["movement_observed"] == false);
}
