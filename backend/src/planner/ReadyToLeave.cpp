#include "planner/ReadyToLeave.hpp"

namespace enro {

ReadyToLeave::ReadyToLeave(nlohmann::json event_types, Router& router, const UrgencyScorer& urgency)
    : event_types_(std::move(event_types)), router_(router), urgency_(urgency) {}

LeavePlan ReadyToLeave::plan(const LeaveInput& in) const {
  // STUB
  // TODO(T07): compute prep, travel (router_.eta * mode_stats.travel_factor), buffer, leave_by and reminder_at
  // per PRD 7.4; urgency from urgency_.score; push one reasoning line per step.
  LeavePlan p;
  p.event_id = in.event.id;
  p.mode = in.mode.empty() ? "walk" : in.mode;
  p.prep_minutes = 20;
  p.travel_minutes = 15;
  p.buffer_minutes = 5;
  p.leave_by = in.event.start_ts - (p.travel_minutes + p.buffer_minutes) * 60;
  p.reminder_at = p.leave_by - p.prep_minutes * 60;
  p.urgency = Urgency::FIRM;
  p.urgency_score = 50;
  p.reasoning = {"[stub] leave 20 min before " + in.event.title};
  return p;
}

}  // namespace enro
