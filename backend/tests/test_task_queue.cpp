// T04: TaskQueue (min-heap + SQLite persistence).
#include <doctest/doctest.h>

#include "scheduler/TaskQueue.hpp"

using namespace enro;

TEST_CASE("TaskQueue constructs on an empty database") {
  Database db(":memory:");
  db.createSchema();
  Repos repos(db);
  TaskQueue q(repos);
  q.load();
  CHECK(q.popDue(0).empty());
}

TEST_CASE("TODO(T04): popDue returns due tasks earliest first and leaves future ones" * doctest::skip()) {}
TEST_CASE("TODO(T04): tasks survive a restart (new TaskQueue on the same DB, load())" * doctest::skip()) {}
TEST_CASE("TODO(T04): nextDueAt is the earliest due_at" * doctest::skip()) {}
