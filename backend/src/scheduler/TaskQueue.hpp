// Min-heap of AgentTask keyed by due_at, persisted to SQLite so a restart loses nothing (PRD 7.1).
// Owner: T04.
#pragma once

#include <mutex>
#include <queue>
#include <vector>

#include "models/Models.hpp"
#include "store/Repos.hpp"

namespace enro {

class TaskQueue {
 public:
  explicit TaskQueue(Repos& repos);

  // Reloads persisted tasks into the heap (call once at boot). Replaces anything in memory.
  void load();
  // Adds a task (and persists it). Returns its row id.
  long long push(const AgentTask& task);
  // Removes and returns every task with due_at <= now, earliest first (ties in insertion order).
  std::vector<AgentTask> popDue(long long now);
  // due_at of the earliest task, or 0 when empty.
  long long nextDueAt() const;
  size_t size() const;

 private:
  struct Item {
    long long due_at;
    long long id;  // SQLite row id, also the tiebreak (insertion order)
    AgentTask task;
    bool operator>(const Item& o) const { return due_at != o.due_at ? due_at > o.due_at : id > o.id; }
  };

  Repos& repos_;
  mutable std::mutex mu_;
  std::priority_queue<Item, std::vector<Item>, std::greater<Item>> heap_;
};

}  // namespace enro
