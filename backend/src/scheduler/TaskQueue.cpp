#include "scheduler/TaskQueue.hpp"

namespace enro {

TaskQueue::TaskQueue(Repos& repos) : repos_(repos) {}

void TaskQueue::load() {
  // STUB
  // TODO(T04): read repos_.tasks() into an in-memory min-heap of (due_at, row id).
}

long long TaskQueue::push(const AgentTask&) {
  // STUB
  // TODO(T04): persist with repos_.insertTask, push (due_at, id) onto the heap.
  return 0;
}

std::vector<AgentTask> TaskQueue::popDue(long long) {
  // STUB
  // TODO(T04): pop while heap.top().due_at <= now, delete each row with repos_.deleteTask.
  return {};
}

long long TaskQueue::nextDueAt() const {
  // STUB
  // TODO(T04): heap.top().due_at.
  return 0;
}

size_t TaskQueue::size() const {
  // STUB
  // TODO(T04): heap size.
  return 0;
}

}  // namespace enro
