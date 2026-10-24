#include "sim/LoadSimulator.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include <thread>

#include "planner/ReadyToLeave.hpp"
#include "planner/Urgency.hpp"
#include "routing/Router.hpp"
#include "util/Time.hpp"

namespace enro {

namespace {

constexpr long long kDemoMidnight = 1792998000;  // Monday 2026-10-26 00:00 America/Los_Angeles
constexpr int kBucketMinutes = 15;
constexpr int kBuckets = 24 * 60 / kBucketMinutes;
constexpr double kAnswerRate = 0.58;    // morning call answer rate (contracts/examples/punctuality.json)
constexpr double kCallMinutes = 0.75;   // average wake or reminder call length
constexpr double kTtsCacheHit = 0.6;    // scripted lines served from AudioCache
constexpr double kPlanChangeEmails = 0.3;  // per user per day that reach the LLM

struct EventMix {
  const char* type;
  double weight;
};
constexpr EventMix kMix[] = {{"class", 0.45}, {"meeting", 0.2}, {"casual", 0.2}, {"exam", 0.07}, {"interview", 0.05},
                             {"flight", 0.03}};

// Per-thread totals, merged at the end so the hot loop shares nothing.
struct Totals {
  long long tasks = 0, calls = 0, llm = 0, tts = 0, reflex = 0;
  double call_minutes = 0;
  std::array<long long, kBuckets> buckets{};
};

// Fixed speeds like FallbackRouter, but over a per-event distance so ETAs and modes vary across users.
struct SimRouter : Router {
  int meters = 1500;
  std::string name() const override { return "sim"; }
  RouteEta eta(const std::string& mode, const Place&, const Place&) override {
    double kmh = mode == "bike" ? 15 : mode == "bus" ? 18 : mode == "drive" ? 35 : 4.8;
    return {std::max(1, static_cast<int>(std::ceil(meters / 1000.0 / kmh * 60.0))), meters, mode, "sim"};
  }
};

int bucketOf(long long ts) {
  long long minute = (ts - kDemoMidnight) / 60;
  return static_cast<int>(std::clamp(minute / kBucketMinutes, 0LL, static_cast<long long>(kBuckets - 1)));
}

// One shard: every user whose wake minute maps to this thread. Plans each user's day with the real
// ReadyToLeave + Urgency and counts what the scheduler would do.
void runShard(const std::vector<int>& wake_minutes, int shard, int threads, Totals& out) {
  SimRouter router;
  UrgencyScorer urgency({});
  ReadyToLeave rtl(nlohmann::json::object(), router, urgency);
  std::mt19937 rng(1234u + static_cast<unsigned>(shard));
  std::uniform_real_distribution<double> unit(0.0, 1.0);
  std::discrete_distribution<int> pick_type({0.45, 0.2, 0.2, 0.07, 0.05, 0.03});
  std::uniform_int_distribution<int> n_events(1, 5), gap(30, 150), duration(50, 120);
  const char* modes[] = {"walk", "walk", "walk", "bike", "bus", "drive"};
  std::uniform_int_distribution<int> pick_mode(0, 5);

  LeaveInput in;
  in.user.timezone = "America/Los_Angeles";

  for (size_t u = 0; u < wake_minutes.size(); ++u) {
    if (wake_minutes[u] % threads != shard) continue;
    long long wake = kDemoMidnight + wake_minutes[u] * 60LL;

    // Morning: plan + wake check, then the ladder until answered (max three calls).
    out.tasks += 2;
    out.reflex += 1;
    for (int attempt = 1; attempt <= 3; ++attempt) {
      long long at = wake + (5 + 3 * (attempt - 1)) * 60LL;
      out.tasks += 1;
      out.reflex += 1;
      out.calls += 1;
      out.buckets[bucketOf(at)] += 1;
      if (unit(rng) < kAnswerRate) {
        out.call_minutes += kCallMinutes;
        out.llm += 2;  // opener + one negotiation turn
        out.tts += 2;
        break;
      }
      out.call_minutes += 0.5;  // rang out
      out.llm += 1;
      out.tts += 1;
    }

    // The day's events.
    int events = n_events(rng);
    long long start = wake + 60 * 60LL + gap(rng) * 60LL;
    in.events_today = events;
    for (int e = 0; e < events; ++e) {
      int len = duration(rng);
      in.event = {"", "", "", "", kMix[pick_type(rng)].type, "calendar", start, start + len * 60LL, false};
      in.mode = modes[pick_mode(rng)];
      in.already_out = e > 0;
      in.type_stats.prep_bias = unit(rng) * 14 - 2;
      in.mode_stats.travel_factor = 0.95 + unit(rng) * 0.3;
      // Destinations 0.5 to 12 km away, mostly close.
      router.meters = static_cast<int>(500 + unit(rng) * unit(rng) * 11500);
      LeavePlan p = rtl.plan(in);

      // LEAVE_REMINDER + DEPARTURE_CHECK + POST_EVENT_LOG per event; reflex decides the channel.
      out.tasks += 3;
      out.reflex += 3;
      if (p.urgency == Urgency::URGENT || (p.urgency == Urgency::FIRM && unit(rng) < 0.3)) {
        out.calls += 1;
        out.call_minutes += kCallMinutes;
        out.buckets[bucketOf(p.reminder_at)] += 1;
        out.llm += 1;
        out.tts += 1;
      } else if (p.urgency == Urgency::FIRM) {
        out.llm += 1;  // earbud line
        out.tts += 1;
      }
      start += (len + gap(rng)) * 60LL;
    }

    // Calendar and Gmail polls (every 15 min over a 16 h day, shared sync tokens), plus email triage.
    out.tasks += 2 * 64;
    out.reflex += 4;
    if (unit(rng) < kPlanChangeEmails) out.llm += 1;
  }
}

std::string hhmmOfBucket(int b) {
  char buf[8];
  std::snprintf(buf, sizeof buf, "%02d:%02d", b * kBucketMinutes / 60, b * kBucketMinutes % 60);
  return buf;
}

double cents(double usd) { return std::round(usd * 100.0) / 100.0; }

}  // namespace

SimResult LoadSimulator::run(int users, int threads) {
  users = std::clamp(users, 0, 2'000'000);
  threads = std::clamp(threads, 1, 64);
  auto t0 = std::chrono::steady_clock::now();

  // Synthetic wake times: around 07:00 (sd 45 min), clamped to 05:00..10:30. Sharding by wake minute
  // spreads the morning spike evenly across workers.
  std::vector<int> wake(static_cast<size_t>(users));
  std::mt19937 rng(42);
  std::normal_distribution<double> around7(7 * 60, 45);
  for (auto& w : wake) w = std::clamp(static_cast<int>(around7(rng)), 5 * 60, 10 * 60 + 30);

  std::vector<Totals> shards(static_cast<size_t>(threads));
  std::vector<std::thread> pool;
  for (int t = 0; t < threads; ++t) pool.emplace_back(runShard, std::cref(wake), t, threads, std::ref(shards[t]));
  for (auto& th : pool) th.join();

  Totals all;
  for (auto& s : shards) {
    all.tasks += s.tasks;
    all.calls += s.calls;
    all.llm += s.llm;
    all.tts += s.tts;
    all.reflex += s.reflex;
    all.call_minutes += s.call_minutes;
    for (int b = 0; b < kBuckets; ++b) all.buckets[b] += s.buckets[b];
  }

  SimResult r;
  r.run_id = "sim_" + std::to_string(time::nowMs());
  r.users = users;
  r.threads = threads;
  r.tasks_scheduled = all.tasks;
  r.llm_calls = all.llm;
  for (int b = 0; b < kBuckets; ++b)
    if (all.buckets[b] > 0) r.calls_per_minute.push_back({hhmmOfBucket(b), static_cast<int>(all.buckets[b])});

  // One simulated day -> per user per month (30 days).
  double per_user = users > 0 ? 30.0 / users : 0.0;
  double llm = all.llm * rates_.llm_per_call * per_user;
  double tts = all.tts * (1.0 - kTtsCacheHit) * rates_.tts_per_call * per_user;
  double phone = all.call_minutes * rates_.telephony_per_min * per_user;
  double reflex = all.reflex * rates_.reflex_per_decision * per_user;
  r.cost_breakdown = {{"llm", cents(llm)}, {"tts", cents(tts)}, {"telephony", cents(phone)}, {"reflex", cents(reflex)}};
  r.cost_per_user_month_usd = 0;
  for (auto& [k, v] : r.cost_breakdown) r.cost_per_user_month_usd += v;
  r.cost_per_user_month_usd = cents(r.cost_per_user_month_usd);

  r.runtime_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
  return r;
}

}  // namespace enro
