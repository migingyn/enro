#include "scheduler/TaskQueue.hpp"

#include "util/Log.hpp"

namespace enro {

TaskQueue::TaskQueue(Repos& repos) : repos_(repos) {}

void TaskQueue::load() {
  auto rows = repos_.tasks();
  std::lock_guard<std::mutex> lock(mu_);
  heap_ = {};
  for (auto& [id, task] : rows) heap_.push({task.due_at, id, std::move(task)});
  LOG_INFO("queue", "loaded " << heap_.size() << " persisted tasks");
}

long long TaskQueue::push(const AgentTask& task) {
  long long id = repos_.insertTask(task);
  std::lock_guard<std::mutex> lock(mu_);
  heap_.push({task.due_at, id, task});
  return id;
}

std::vector<AgentTask> TaskQueue::popDue(long long now) {
  std::vector<Item> due;
  {
    std::lock_guard<std::mutex> lock(mu_);
    while (!heap_.empty() && heap_.top().due_at <= now) {
      due.push_back(heap_.top());
      heap_.pop();
    }
  }
  std::vector<AgentTask> out;
  out.reserve(due.size());
  for (auto& item : due) {
    repos_.deleteTask(item.id);
    out.push_back(std::move(item.task));
  }
  return out;
}

long long TaskQueue::nextDueAt() const {
  std::lock_guard<std::mutex> lock(mu_);
  return heap_.empty() ? 0 : heap_.top().due_at;
}

size_t TaskQueue::size() const {
  std::lock_guard<std::mutex> lock(mu_);
  return heap_.size();
}

}  // namespace enro
