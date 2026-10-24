// Morning escalation ladder (PRD 7.6) and emergency contact notify. Owner: T10.
//   alarm + grace (5 min), no awake signal -> CALL_1 (calm)
//   + 3 min, no answer                     -> CALL_2 (louder voice, harsher persona)
//   + 3 min, no answer                     -> CALL_3 (full yelling) AND notify contacts in parallel
//   answer at any step                     -> awake check ("What's your first thing today?")
//   pass awake check                       -> AWAKE, schedule a 5 min verification call
#pragma once

#include <string>

#include "models/Models.hpp"

namespace enro {

struct AppContext;

class WakeLadder {
 public:
  explicit WakeLadder(AppContext& ctx) : ctx_(ctx) {}

  // Starts the ladder for user now (demo trigger) and returns the first call's id.
  std::string start(const std::string& user_id);
  // The ladder steps for a day, shown in the plan's `wake` block.
  WakePlan plan(const User& user, const std::string& date) const;
  // Called when a ladder call is declined or not answered.
  void onNoAnswer(const std::string& call_id);
  // Called when the user passes the awake check.
  void onAwake(const std::string& call_id);

  // Places one call right now through the voice provider, records it, and broadcasts call_state.
  // Shared helper: leave reminders and departure checks use it too.
  std::string callNow(const std::string& user_id, const std::string& purpose, int attempt);

 private:
  AppContext& ctx_;
};

}  // namespace enro
