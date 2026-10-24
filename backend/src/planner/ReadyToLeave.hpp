// Ready-to-Leave Engine (PRD 7.4). Owner: T07.
//   prep_minutes   = base_prep[event_type] + learned_prep_bias[event_type]
//   travel_minutes = eta(mode_for(event), origin, destination) * learned_travel_factor[mode]
//   leave_by       = event.start - travel_minutes - buffer
//   reminder_at    = leave_by - prep_minutes
// Every step writes a reasoning line (see contracts/examples/plan.json for the expected wording).
#pragma once

#include <nlohmann/json.hpp>

#include "models/Models.hpp"
#include "planner/Urgency.hpp"
#include "routing/Router.hpp"

namespace enro {

struct LeaveInput {
  User user;
  Event event;
  Place origin;
  std::string mode;              // from TransportLearner (T13) or the user's preferred_mode
  PunctualityStats type_stats;   // learned, for event.event_type
  PunctualityStats mode_stats;   // learned, for mode
  int events_today = 1;
  long long now = 0;
};

class ReadyToLeave {
 public:
  // event_types: backend/config/event_types.json (base_prep_minutes, buffer_minutes per type).
  ReadyToLeave(nlohmann::json event_types, Router& router, const UrgencyScorer& urgency);

  LeavePlan plan(const LeaveInput& in) const;

 private:
  nlohmann::json event_types_;
  Router& router_;
  const UrgencyScorer& urgency_;
};

}  // namespace enro
