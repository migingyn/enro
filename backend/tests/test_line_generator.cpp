// T12: LineGenerator + personas.
#include <doctest/doctest.h>

#include "ai/FakeLlm.hpp"
#include "ai/LineGenerator.hpp"

using namespace enro;

TEST_CASE("canned lines fill placeholders, generate never returns empty") {
  FakeLlm llm;
  nlohmann::json scripts = {{"lines", {{"wake_call_1", {"Morning {name}, {event} in {minutes}."}}}}};
  LineGenerator g(llm, {}, scripts);
  LineRequest r{"wake_call_1", "best_friend", Urgency::CHILL, "Sam", {{"event", "CSE 110"}, {"minutes", 60}}};
  CHECK(g.canned(r) == "Morning Sam, CSE 110 in 60.");
  CHECK_FALSE(g.generate({"no_such_key", "coach", Urgency::URGENT, "Sam", {}}).empty());
}

TEST_CASE("TODO(T12): URGENT lines are harsher and shorter than CHILL" * doctest::skip()) {}
TEST_CASE("TODO(T12): persona never threatens anything but calling emergency contacts" * doctest::skip()) {}
