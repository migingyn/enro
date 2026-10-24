// One loop pops due tasks and dispatches them to handlers by kind. Handlers may push new
// tasks. Every action writes a Decision row for the dashboard (PRD 7.1). Owner: T04.
#pragma once

#include <atomic>
#include <functional>
#include <map>
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

  void start();  // background loop
  void stop();
  // Runs one iteration: pops tasks due at `now` and dispatches them. Returns how many ran.
  // The loop calls this; tests call it directly with a fake clock.
  int tick(long long now);

 private:
  TaskQueue& queue_;
  std::map<TaskKind, Handler> handlers_;
  std::thread thread_;
  std::atomic<bool> running_{false};
};

}  // namespace enro
