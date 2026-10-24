#include "scheduler/Scheduler.hpp"

#include "util/Log.hpp"

namespace enro {

Scheduler::Scheduler(TaskQueue& queue) : queue_(queue) {}

Scheduler::~Scheduler() { stop(); }

void Scheduler::on(TaskKind kind, Handler handler) { handlers_[kind] = std::move(handler); }

void Scheduler::start() {
  // STUB
  // TODO(T04): queue_.load(), then a thread that sleeps until queue_.nextDueAt() (or ~1 s) and calls tick(now).
  LOG_INFO("scheduler", "scheduler loop not implemented yet (T04)");
}

void Scheduler::stop() {
  running_ = false;
  if (thread_.joinable()) thread_.join();
}

int Scheduler::tick(long long) {
  // STUB
  // TODO(T04): for each task in queue_.popDue(now), call handlers_[task.kind]; log and skip unknown kinds;
  // catch handler exceptions so one bad task never stops the loop.
  return 0;
}

}  // namespace enro
