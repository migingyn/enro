// T16: Load simulator.
#include <doctest/doctest.h>

#include <numeric>

#include "sim/LoadSimulator.hpp"

using namespace enro;

TEST_CASE("run echoes users and threads") {
  LoadSimulator s;
  auto r = s.run(100, 2);
  CHECK(r.users == 100);
  CHECK(r.threads == 2);
}

TEST_CASE("100,000 users finish under 5 seconds") {
  LoadSimulator s;
  auto r = s.run(100000, 8);
  CHECK(r.runtime_ms < 5000);
  CHECK(r.tasks_scheduled > 100000 * 10);
  CHECK(r.llm_calls > 100000);
  MESSAGE("100k users: " << r.runtime_ms << " ms, " << r.tasks_scheduled << " tasks, $" << r.cost_per_user_month_usd
                         << "/user/month");
}

TEST_CASE("calls_per_minute sums to the scheduled call count, and the morning spike is around 07:00") {
  LoadSimulator s;
  auto r = s.run(20000, 4);
  long long calls = 0;
  int peak = 0;
  std::string peak_at;
  for (auto& b : r.calls_per_minute) {
    calls += b.calls;
    if (b.calls > peak) peak = b.calls, peak_at = b.minute;
  }
  // Every user gets at least one wake call, at most three, plus some reminder calls.
  CHECK(calls >= 20000);
  CHECK(calls <= 20000 * 3 + 20000 * 5);
  CHECK(peak_at >= "06:30");
  CHECK(peak_at <= "07:45");
}

TEST_CASE("cost_per_user_month equals the sum of cost_breakdown") {
  LoadSimulator s;
  auto r = s.run(5000, 2);
  double sum = 0;
  for (auto& [k, v] : r.cost_breakdown) sum += v;
  CHECK(r.cost_per_user_month_usd == doctest::Approx(sum).epsilon(0.001));
  CHECK(r.cost_breakdown.size() == 4);
  CHECK(r.cost_breakdown.at("telephony") > 0);
}

TEST_CASE("same input gives the same counts regardless of timing") {
  LoadSimulator s;
  auto a = s.run(3000, 3), b = s.run(3000, 3);
  CHECK(a.tasks_scheduled == b.tasks_scheduled);
  CHECK(a.llm_calls == b.llm_calls);
}
