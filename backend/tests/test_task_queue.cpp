// T04: TaskQueue (min-heap + SQLite persistence).
#include <doctest/doctest.h>

#include "scheduler/TaskQueue.hpp"

using namespace enro;

namespace {
AgentTask task(long long due, TaskKind kind = TaskKind::WAKE_CALL, nlohmann::json payload = nlohmann::json::object()) {
  return {due, "u_demo", kind, std::move(payload)};
}
}  // namespace

TEST_CASE("TaskQueue constructs on an empty database") {
  Database db(":memory:");
  db.createSchema();
  Repos repos(db);
  TaskQueue q(repos);
  q.load();
  CHECK(q.popDue(0).empty());
  CHECK(q.size() == 0);
  CHECK(q.nextDueAt() == 0);
}

TEST_CASE("popDue returns due tasks earliest first and leaves future ones") {
  Database db(":memory:");
  db.createSchema();
  Repos repos(db);
  TaskQueue q(repos);
  q.push(task(300, TaskKind::LEAVE_REMINDER));
  q.push(task(100, TaskKind::WAKE_CHECK));
  q.push(task(200, TaskKind::WAKE_CALL, {{"attempt", 2}}));
  q.push(task(900));
  CHECK(q.size() == 4);
  CHECK(q.nextDueAt() == 100);

  auto due = q.popDue(300);
  REQUIRE(due.size() == 3);
  CHECK(due[0].kind == TaskKind::WAKE_CHECK);
  CHECK(due[1].payload["attempt"] == 2);
  CHECK(due[2].kind == TaskKind::LEAVE_REMINDER);
  CHECK(q.size() == 1);
  CHECK(q.nextDueAt() == 900);
  CHECK(repos.tasks().size() == 1);  // popped rows are deleted
}

TEST_CASE("tasks survive a restart") {
  Database db(":memory:");
  db.createSchema();
  Repos repos(db);
  {
    TaskQueue q(repos);
    q.push(task(50));
    q.push(task(10, TaskKind::POLL_GMAIL));
  }
  TaskQueue restarted(repos);
  restarted.load();
  CHECK(restarted.size() == 2);
  auto due = restarted.popDue(1000);
  REQUIRE(due.size() == 2);
  CHECK(due[0].kind == TaskKind::POLL_GMAIL);
}

TEST_CASE("same due_at pops in insertion order") {
  Database db(":memory:");
  db.createSchema();
  Repos repos(db);
  TaskQueue q(repos);
  q.push(task(100, TaskKind::WAKE_CALL, {{"n", 1}}));
  q.push(task(100, TaskKind::WAKE_CALL, {{"n", 2}}));
  auto due = q.popDue(100);
  REQUIRE(due.size() == 2);
  CHECK(due[0].payload["n"] == 1);
  CHECK(due[1].payload["n"] == 2);
}
