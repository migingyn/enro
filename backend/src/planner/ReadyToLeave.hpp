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
  Place destination;              // event.location with coordinates when known (Places); label-only otherwise
  std::string mode;              // from TransportLearner (T13) or the user's preferred_mode
  PunctualityStats type_stats;   // learned, for event.event_type
  PunctualityStats mode_stats;   // learned, for mode
  PunctualityStats walk_stats;   // learned, for walking legs of transit trips (to and from the stop)
  int events_today = 1;
  long long now = 0;             // 0 = planning ahead (slack counts as neutral)
  // Set when the user is already out (at the previous event): no prep, travel from previous_location.
  bool already_out = false;
  std::string after_title;       // previous event's title, for the reasoning line
};

class ReadyToLeave {
 public:
  // event_types: backend/config/event_types.json (base_prep_minutes, buffer_minutes per type).
  ReadyToLeave(nlohmann::json event_types, Router& router, const UrgencyScorer& urgency);

  LeavePlan plan(const LeaveInput& in) const;

  // From event_types.json, falling back to PRD defaults when the type or file is missing.
  int basePrep(const std::string& event_type) const;
  int buffer(const std::string& event_type) const;
  static bool hardStart(const std::string& event_type);

 private:
  nlohmann::json event_types_;
  Router& router_;
  const UrgencyScorer& urgency_;
};

}  // namespace enro
