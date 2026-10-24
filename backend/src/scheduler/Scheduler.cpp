#include "scheduler/Scheduler.hpp"

#include <algorithm>
#include <chrono>

#include "models/Json.hpp"
#include "util/Log.hpp"
#include "util/Time.hpp"

namespace enro {

namespace {
// The loop never sleeps longer than this, so a task pushed without wake() still fires within a second.
constexpr long long kMaxSleepMs = 1000;
}  // namespace

Scheduler::Scheduler(TaskQueue& queue) : queue_(queue) {}

Scheduler::~Scheduler() { stop(); }

void Scheduler::on(TaskKind kind, Handler handler) { handlers_[kind] = std::move(handler); }

void Scheduler::start() {
  if (running_) return;
  queue_.load();
  running_ = true;
  thread_ = std::thread([this] { loop(); });
  LOG_INFO("scheduler", "started with " << queue_.size() << " queued tasks");
}

void Scheduler::stop() {
  if (!running_.exchange(false)) return;
  wake();
  if (thread_.joinable()) thread_.join();
}

void Scheduler::wake() {
  {
    std::lock_guard<std::mutex> lock(mu_);
    woken_ = true;
  }
  cv_.notify_all();
}

int Scheduler::tick(long long now) {
  int ran = 0;
  for (auto& task : queue_.popDue(now)) {
    auto it = handlers_.find(task.kind);
    std::string kind = nlohmann::json(task.kind).get<std::string>();
    if (it == handlers_.end()) {
      LOG_WARN("scheduler", "no handler for " << kind << ", dropping task for " << task.user_id);
      continue;
    }
    try {
      it->second(task);
      ++ran;
    } catch (const std::exception& e) {
      LOG_ERROR("scheduler", kind << " for " << task.user_id << " failed: " << e.what());
    }
  }
  return ran;
}

void Scheduler::loop() {
  while (running_) {
    tick(time::nowTs());
    long long now_ms = time::nowMs();
    long long next = queue_.nextDueAt();
    long long wait_ms = next == 0 ? kMaxSleepMs : std::clamp(next * 1000 - now_ms, 0LL, kMaxSleepMs);
    std::unique_lock<std::mutex> lock(mu_);
    cv_.wait_for(lock, std::chrono::milliseconds(wait_ms), [this] { return woken_ || !running_; });
    woken_ = false;
  }
}

}  // namespace enro
