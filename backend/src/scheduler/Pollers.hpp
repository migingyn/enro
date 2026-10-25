// POLL_CALENDAR and POLL_GMAIL (PRD 7.1, 7.2): feed calendar and inbox data through the normal pipeline
// every POLL_SECONDS, whichever source is configured (seed files now, Google once T03 lands).
//   calendar -> new or source-changed events into SQLite -> plan broadcast
//   inbox    -> EmailTriage (reflex, extraction, event update, plan broadcast)
// Owner: T04.
#pragma once

#include <string>

namespace enro {

struct AppContext;
class Scheduler;
struct AgentTask;

class Pollers {
 public:
  explicit Pollers(AppContext& ctx) : ctx_(ctx) {}

  void registerHandlers(Scheduler& scheduler);
  // Queues the first calendar and inbox poll for every user, due at `now`. Call once per boot.
  // Tasks left in SQLite by earlier boots become stale and are dropped, so polls never multiply.
  void start(long long now);

  // One poll each. Return how many events changed / messages were triaged.
  int pollCalendar(const std::string& user_id, long long now);
  int pollInbox(const std::string& user_id);

 private:
  void handle(const AgentTask& task);
  int intervalSeconds() const;

  AppContext& ctx_;
};

}  // namespace enro
