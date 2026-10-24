// T04: Scheduler loop and dispatch.
#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <thread>

#include "scheduler/Scheduler.hpp"
#include "util/Time.hpp"

using namespace enro;

namespace {
struct Fixture {
  Database db{":memory:"};
  Repos repos{db};
  TaskQueue q{repos};
  Fixture() { db.createSchema(); }
};
}  // namespace

TEST_CASE("tick dispatches each due task to the handler for its kind") {
  Fixture f;
  Scheduler s(f.q);
  std::vector<std::string> ran;
  s.on(TaskKind::WAKE_CALL, [&](const AgentTask& t) { ran.push_back("wake:" + t.user_id); });
  s.on(TaskKind::POLL_GMAIL, [&](const AgentTask&) { ran.push_back("gmail"); });
  f.q.push({10, "u1", TaskKind::WAKE_CALL, {}});
  f.q.push({20, "u1", TaskKind::POLL_GMAIL, {}});
  f.q.push({99, "u1", TaskKind::WAKE_CALL, {}});
  CHECK(s.tick(50) == 2);
  CHECK(ran == std::vector<std::string>{"wake:u1", "gmail"});
  CHECK(s.tick(50) == 0);
  CHECK(s.tick(100) == 1);
}

TEST_CASE("unknown kinds are skipped and a throwing handler doesn't stop other tasks") {
  Fixture f;
  Scheduler s(f.q);
  int ok = 0;
  s.on(TaskKind::WAKE_CALL, [](const AgentTask&) { throw std::runtime_error("boom"); });
  s.on(TaskKind::LEAVE_REMINDER, [&](const AgentTask&) { ++ok; });
  f.q.push({1, "u", TaskKind::WAKE_CALL, {}});
  f.q.push({2, "u", TaskKind::POST_EVENT_LOG, {}});  // no handler
  f.q.push({3, "u", TaskKind::LEAVE_REMINDER, {}});
  s.tick(10);
  CHECK(ok == 1);
  CHECK(f.q.size() == 0);
}

TEST_CASE("handlers can push follow-up tasks") {
  Fixture f;
  Scheduler s(f.q);
  s.on(TaskKind::WAKE_CALL, [&](const AgentTask& t) {
    int attempt = t.payload.value("attempt", 1);
    if (attempt < 3) f.q.push({t.due_at + 180, t.user_id, TaskKind::WAKE_CALL, {{"attempt", attempt + 1}}});
  });
  f.q.push({0, "u", TaskKind::WAKE_CALL, {{"attempt", 1}}});
  CHECK(s.tick(0) == 1);
  CHECK(s.tick(180) == 1);
  CHECK(s.tick(360) == 1);
  CHECK(s.tick(10000) == 0);
}

TEST_CASE("the background loop fires a task that comes due while running") {
  Fixture f;
  Scheduler s(f.q);
  std::atomic<int> fired{0};
  s.on(TaskKind::DEPARTURE_CHECK, [&](const AgentTask&) { ++fired; });
  s.start();
  f.q.push({time::nowTs(), "u", TaskKind::DEPARTURE_CHECK, {}});
  s.wake();
  for (int i = 0; i < 100 && fired == 0; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(20));
  s.stop();
  CHECK(fired == 1);
}
