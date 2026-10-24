// T04: Scheduler loop and dispatch.
#include <doctest/doctest.h>

#include "scheduler/Scheduler.hpp"

using namespace enro;

TEST_CASE("Scheduler registers handlers and ticks without crashing") {
  Database db(":memory:");
  db.createSchema();
  Repos repos(db);
  TaskQueue q(repos);
  Scheduler s(q);
  s.on(TaskKind::WAKE_CALL, [](const AgentTask&) {});
  CHECK(s.tick(0) == 0);
}

TEST_CASE("TODO(T04): tick dispatches each due task to the handler for its kind" * doctest::skip()) {}
TEST_CASE("TODO(T04): a throwing handler doesn't stop other tasks" * doctest::skip()) {}
TEST_CASE("TODO(T04): handlers can push follow-up tasks" * doctest::skip()) {}
