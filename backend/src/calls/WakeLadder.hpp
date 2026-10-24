// Morning escalation ladder (PRD 7.6) and emergency contact notify. Owner: T10.
//   alarm + grace (5 min), no awake signal -> CALL_1 (calm)
//   + 3 min, no answer                     -> CALL_2 (louder voice, harsher persona)
//   + 3 min, no answer                     -> CALL_3 (full yelling) AND notify contacts in parallel
//   answer at any step                     -> awake check ("What's your first thing today?")
//   pass awake check                       -> AWAKE, schedule a 5 min verification call
//
// Each step is a WAKE_CALL task on the scheduler (T04). Ladder state lives in kv "ladder:<user_id>",
// so a restart picks up where it left off. Tasks from an older ladder, or for an attempt that was
// already pulled forward, are ignored.
#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "models/Models.hpp"

namespace enro {

struct AppContext;
class Scheduler;

struct LadderState {
  std::string ladder_id;
  int next_attempt = 1;  // the attempt the next WAKE_CALL must carry to be acted on
  bool awake = false;
  bool demo = false;     // demo ladders retry fast after a decline
  long long gap_s = 180;
  std::string last_call_id;
};

class WakeLadder {
 public:
  explicit WakeLadder(AppContext& ctx) : ctx_(ctx) {}

  // Demo trigger: rings CALL_1 now, then escalates every LADDER_DEMO_GAP_SECONDS (default 20).
  // Returns the first call's id.
  std::string start(const std::string& user_id);
  // Real mornings: queues CALL_1 at alarm + grace on `date`, 3 minutes between steps.
  // Called the night before (PLAN_TOMORROW). Returns the ladder id.
  std::string schedule(const std::string& user_id, const std::string& date);
  // The ladder steps for a day, shown in the plan's `wake` block.
  WakePlan plan(const User& user, const std::string& date) const;

  // Called by Conversation (T11).
  void onNoAnswer(const std::string& call_id);
  void onAwake(const std::string& call_id);

  // Registers the WAKE_CALL handler. Called once from buildApp.
  void registerHandlers(Scheduler& scheduler);
  void handleWakeCall(const AgentTask& task);

  // Places one call right now through the voice provider, records it, and broadcasts call_state.
  // Shared helper: leave reminders and departure checks use it too.
  std::string callNow(const std::string& user_id, const std::string& purpose, int attempt);

  std::string lastCallId(const std::string& user_id) const;

 private:
  LadderState load(const std::string& user_id) const;
  void save(const std::string& user_id, const LadderState& s);
  void queueAttempt(const std::string& user_id, const LadderState& s, int attempt, long long due_at);
  // "8:00" for the first event today, or "" when nothing is on.
  std::string firstThingHint(const std::string& user_id) const;

  AppContext& ctx_;
};

}  // namespace enro
