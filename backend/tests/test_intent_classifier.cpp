// T11: IntentClassifier.
#include <doctest/doctest.h>

#include "ai/FakeLlm.hpp"
#include "ai/IntentClassifier.hpp"

using namespace enro;

TEST_CASE("classify returns a label") {
  FakeLlm llm;
  IntentClassifier c(llm);
  CHECK_FALSE(c.classify("hello").label.empty());
}

TEST_CASE("TODO(T11): 'five more minutes' -> SNOOZE_REQUEST, minutes 5" * doctest::skip()) {}
TEST_CASE("TODO(T11): 'I'm up' -> AWAKE; 'leaving now' -> LEFT" * doctest::skip()) {}
