// One loop pops due tasks and dispatches them to handlers by kind. Handlers may push new
// tasks. Every action writes a Decision row for the dashboard (PRD 7.1). Owner: T04.
#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <thread>

#include "scheduler/TaskQueue.hpp"

namespace enro {

class Scheduler {
 public:
  using Handler = std::function<void(const AgentTask&)>;

  explicit Scheduler(TaskQueue& queue);
  ~Scheduler();

  // Register before start(). One handler per kind; later registration replaces earlier.
  void on(TaskKind kind, Handler handler);

  // Loads persisted tasks and starts the background loop.
  void start();
  void stop();
  // Wakes the loop now (call after pushing a task that may be due sooner than the next one).
  void wake();
  // Runs one iteration: pops tasks due at `now` and dispatches them. Returns how many ran.
  // The loop calls this; tests call it directly with a fake clock.
  int tick(long long now);

 private:
  void loop();

  TaskQueue& queue_;
  std::map<TaskKind, Handler> handlers_;
  std::thread thread_;
  std::atomic<bool> running_{false};
  std::mutex mu_;
  std::condition_variable cv_;
  bool woken_ = false;
};

}  // namespace enro
