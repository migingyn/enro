// util/: Time, Ewma, Log. Shared.
#include <doctest/doctest.h>

#include "util/Ewma.hpp"
#include "util/Time.hpp"

using namespace enro;

TEST_CASE("demo day timestamps match the fixtures (America/Los_Angeles)") {
  CHECK(time::midnightIn("2026-10-26", "America/Los_Angeles") == 1792998000);
  CHECK(time::atLocalTime("2026-10-26", "07:00", "America/Los_Angeles") == 1793023200);
  CHECK(time::hhmmIn(1793026800, "America/Los_Angeles") == "08:00");
  CHECK(time::dateIn(1793026800, "America/Los_Angeles") == "2026-10-26");
}

TEST_CASE("Ewma: first sample sets the value, later ones blend") {
  Ewma e(0.5);
  e.add(10);
  CHECK(e.value() == doctest::Approx(10));
  e.add(20);
  CHECK(e.value() == doctest::Approx(15));
  CHECK(e.samples() == 2);
}
