// T16: Load simulator.
#include <doctest/doctest.h>

#include "sim/LoadSimulator.hpp"

using namespace enro;

TEST_CASE("run echoes users and threads") {
  LoadSimulator s;
  auto r = s.run(100, 2);
  CHECK(r.users == 100);
  CHECK(r.threads == 2);
}

TEST_CASE("TODO(T16): 100,000 users finish under 5 seconds" * doctest::skip()) {}
TEST_CASE("TODO(T16): calls_per_minute sums to the scheduled call count" * doctest::skip()) {}
TEST_CASE("TODO(T16): cost_per_user_month equals the sum of cost_breakdown" * doctest::skip()) {}
