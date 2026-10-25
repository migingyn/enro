#include "scheduler/Pollers.hpp"

#include <functional>

#include "app/App.hpp"
#include "models/Json.hpp"
#include "planner/DayPlanner.hpp"
#include "util/Log.hpp"

namespace enro {

namespace {

constexpr long long kLookbackS = 86400;        // a day back
constexpr long long kLookaheadS = 30 * 86400;  // a month ahead

std::string genKey(TaskKind kind, const std::string& user_id) {
  return "poll:" + nlohmann::json(kind).get<std::string>() + ":" + user_id;
}
std::string sourceKey(const std::string& event_id) { return "calsrc:" + event_id; }
std::string hashOf(const Event& e) { return std::to_string(std::hash<std::string>{}(nlohmann::json(e).dump())); }

}  // namespace

int Pollers::intervalSeconds() const { return std::max(5, ctx_.env.getInt("POLL_SECONDS", 60)); }

void Pollers::registerHandlers(Scheduler& scheduler) {
  scheduler.on(TaskKind::POLL_CALENDAR, [this](const AgentTask& t) { handle(t); });
  scheduler.on(TaskKind::POLL_GMAIL, [this](const AgentTask& t) { handle(t); });
}

void Pollers::start(long long now) {
  std::string gen = std::to_string(now);
  for (auto& u : ctx_.repos->users()) {
    for (TaskKind kind : {TaskKind::POLL_CALENDAR, TaskKind::POLL_GMAIL}) {
      ctx_.repos->setKv(genKey(kind, u.id), gen);
      ctx_.queue->push({now, u.id, kind, {{"gen", gen}}});
    }
  }
  ctx_.scheduler->wake();
  LOG_INFO("poll", "calendar (" << ctx_.calendar->name() << ") and inbox (" << ctx_.email->name() << ") every "
                                << intervalSeconds() << " s");
}

void Pollers::handle(const AgentTask& task) {
  auto gen = ctx_.repos->kv(genKey(task.kind, task.user_id));
  if (!gen || task.payload.value("gen", "") != *gen) return;  // left over from an earlier boot

  if (task.kind == TaskKind::POLL_CALENDAR) pollCalendar(task.user_id, task.due_at);
  else pollInbox(task.user_id);

  ctx_.queue->push({task.due_at + intervalSeconds(), task.user_id, task.kind, task.payload});
}

int Pollers::pollCalendar(const std::string& user_id, long long now) {
  int changed = 0;
  for (auto& e : ctx_.calendar->events(user_id, now - kLookbackS, now + kLookaheadS)) {
    std::string h = hashOf(e);
    auto seen = ctx_.repos->kv(sourceKey(e.id));
    bool exists = ctx_.repos->event(e.id).has_value();
    if (exists && (!seen || *seen == h)) {
      // Unchanged at the source (or first sight of a seeded event): keep our copy, which may carry changes
      // that came from email triage (a canceled section, a moved room).
      if (!seen) ctx_.repos->setKv(sourceKey(e.id), h);
      continue;
    }
    ctx_.repos->upsertEvent(e);  // new event, or the calendar itself changed it
    ctx_.repos->setKv(sourceKey(e.id), h);
    ++changed;
  }
  if (changed > 0) {
    LOG_INFO("poll", "calendar: " << changed << " new or changed event(s) for " << user_id);
    ctx_.dashboard->broadcast("plan", DayPlanner(ctx_).build(user_id));
  }
  return changed;
}

int Pollers::pollInbox(const std::string& user_id) {
  auto messages = ctx_.email->poll(user_id);
  if (messages.empty()) return 0;
  ctx_.triage->process(user_id, messages);  // records decisions; broadcasts the plan if it changed
  LOG_INFO("poll", "inbox: triaged " << messages.size() << " new message(s) for " << user_id);
  return static_cast<int>(messages.size());
}

}  // namespace enro
