// Min-heap of AgentTask keyed by due_at, persisted to SQLite so a restart loses nothing (PRD 7.1).
// Owner: T04.
#pragma once

#include <mutex>
#include <vector>

#include "models/Models.hpp"
#include "store/Repos.hpp"

namespace enro {

class TaskQueue {
 public:
  explicit TaskQueue(Repos& repos);

  // Reloads persisted tasks into the heap (call once at boot).
  void load();
  // Adds a task (and persists it). Returns its row id.
  long long push(const AgentTask& task);
  // Removes and returns every task with due_at <= now, earliest first.
  std::vector<AgentTask> popDue(long long now);
  // due_at of the earliest task, or 0 when empty.
  long long nextDueAt() const;
  size_t size() const;

 private:
  Repos& repos_;
  mutable std::mutex mu_;
};

}  // namespace enro
