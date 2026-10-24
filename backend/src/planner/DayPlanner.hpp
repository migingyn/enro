// Builds a user's DayPlan: today's events, a LeavePlan per event from ReadyToLeave with learned stats,
// and the wake block from the WakeLadder. Used by GET /plan, /consequences, and call openers.
// Owner: T07. Stateless, so construct it where needed: DayPlanner(ctx).build(...).
#pragma once

#include <string>

#include "models/Models.hpp"

namespace enro {

struct AppContext;

class DayPlanner {
 public:
  explicit DayPlanner(AppContext& ctx) : ctx_(ctx) {}

  // date "YYYY-MM-DD" in the user's timezone. Empty = defaultDate().
  DayPlan build(const std::string& user_id, const std::string& date = "");
  // Today if it has events, else the next day with events (the seeded demo day), else today.
  std::string defaultDate(const std::string& user_id) const;
  // The day's plan plus consequences of running delay_minutes behind it.
  ConsequenceReport consequences(const std::string& user_id, int delay_minutes, const std::string& date = "");

 private:
  AppContext& ctx_;
};

}  // namespace enro
