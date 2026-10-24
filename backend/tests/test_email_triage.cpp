// T09: Email triage (PRD 7.2).
#include <doctest/doctest.h>

#include "TestApp.hpp"

using namespace enro;

TEST_CASE("process writes one email_relevance decision per message") {
  auto app = test::fakeApp();
  auto out = app->triage->process("u_demo", {{"m1", "a@b.c", "Hello", "hi", 0}, {"m2", "x@y.z", "Promo", "sale", 0}});
  CHECK(out.size() == 2);
  CHECK(app->repos->recentDecisions().size() == 2);
}

TEST_CASE("TODO(T09): 'Section canceled today' -> PLAN_CHANGE, cancels evt_cse110_dis, plan shifts" * doctest::skip()) {}
TEST_CASE("TODO(T09): IGNORE emails never reach the LLM" * doctest::skip()) {}
TEST_CASE("TODO(T09): the same message id is not parsed twice (LRU)" * doctest::skip()) {}
TEST_CASE("TODO(T09): only the extracted fact is stored, never the body" * doctest::skip()) {}
