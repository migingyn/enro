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

TEST_CASE("'five more minutes' -> SNOOZE_REQUEST, minutes 5") {
  FakeLlm llm;
  IntentClassifier c(llm);
  auto i = c.classify("Give me five more minutes.");
  CHECK(i.label == "SNOOZE_REQUEST");
  CHECK(i.minutes == 5);
  CHECK(c.classify("10 more min please").minutes == 10);
  CHECK(c.classify("half an hour").minutes == 30);
  CHECK(c.classify("snooze").minutes == 5);
}

TEST_CASE("'I'm up' -> AWAKE; 'leaving now' -> LEFT") {
  FakeLlm llm;
  IntentClassifier c(llm);
  CHECK(c.classify("Okay, I'm up").label == "AWAKE");
  CHECK(c.classify("I'm leaving now").label == "LEFT");
  CHECK(c.classify("getting dressed").label == "READY");
  CHECK(c.classify("ugh no").label == "STALLING");
  CHECK(c.classify("what happens if I'm 10 minutes late").label == "ASK_CONSEQUENCES");
  CHECK(c.classify("CSE 110 at eight").label == "UNCLEAR");
}

TEST_CASE("'no' inside other words is not STALLING") {
  CHECK(IntentClassifier::keywords("I know, I know").label != "STALLING");
}
