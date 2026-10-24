// Consequence Engine (PRD 7.5). Given the plan and a proposed delay, returns each affected event
// with OK | AT_RISK | MISSED and minutes late. Used in negotiation, call openers, and the dashboard.
// Owner: T08.
#pragma once

#include <vector>

#include "models/Models.hpp"

namespace enro {

class ConsequenceEngine {
 public:
  // events and plans are the day's plan (same order not required; match by event_id).
  ConsequenceReport evaluate(const std::string& user_id, const std::vector<Event>& events,
                             const std::vector<LeavePlan>& plans, int delay_minutes) const;
};

}  // namespace enro
