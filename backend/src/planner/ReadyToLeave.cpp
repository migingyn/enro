#include "planner/ReadyToLeave.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>

#include "util/Time.hpp"

namespace enro {

namespace {

struct TypeDefaults {
  int prep, buffer;
};

// Used when backend/config/event_types.json is missing a type. Interview matches plan.json (45 + 15).
const std::map<std::string, TypeDefaults>& typeDefaults() {
  static const std::map<std::string, TypeDefaults> d = {{"class", {20, 5}},   {"interview", {45, 15}},
                                                        {"exam", {30, 15}},   {"flight", {60, 30}},
                                                        {"meeting", {15, 5}}, {"casual", {15, 5}}};
  return d;
}

std::string signedInt(long v) { return (v >= 0 ? "+" : "") + std::to_string(v); }

std::string factor2(double f) {
  char buf[16];
  std::snprintf(buf, sizeof buf, "%.2f", f);
  return buf;
}

}  // namespace

ReadyToLeave::ReadyToLeave(nlohmann::json event_types, Router& router, const UrgencyScorer& urgency)
    : event_types_(std::move(event_types)), router_(router), urgency_(urgency) {}

int ReadyToLeave::basePrep(const std::string& type) const {
  if (event_types_.contains("types") && event_types_["types"].contains(type))
    return event_types_["types"][type].value("base_prep_minutes", 20);
  auto it = typeDefaults().find(type);
  return it == typeDefaults().end() ? 20 : it->second.prep;
}

int ReadyToLeave::buffer(const std::string& type) const {
  if (event_types_.contains("types") && event_types_["types"].contains(type))
    return event_types_["types"][type].value("buffer_minutes", 5);
  auto it = typeDefaults().find(type);
  return it == typeDefaults().end() ? 5 : it->second.buffer;
}

bool ReadyToLeave::hardStart(const std::string& type) { return type == "interview" || type == "exam" || type == "flight"; }

LeavePlan ReadyToLeave::plan(const LeaveInput& in) const {
  const Event& e = in.event;
  const std::string tz = in.user.timezone.empty() ? "America/Los_Angeles" : in.user.timezone;
  auto hhmm = [&](long long ts) { return time::hhmmIn(ts, tz); };

  LeavePlan p;
  p.event_id = e.id;
  p.mode = in.mode.empty() ? "walk" : in.mode;

  // prep_minutes = base_prep[event_type] + learned_prep_bias[event_type]
  if (in.already_out) {
    p.prep_minutes = 0;
    p.reasoning.push_back("prep 0 min (already on campus" + (in.after_title.empty() ? "" : " after " + in.after_title) + ")");
  } else {
    int base = basePrep(e.event_type);
    long bias = std::lround(in.type_stats.prep_bias);
    p.prep_minutes = std::max(0, base + static_cast<int>(bias));
    p.reasoning.push_back("prep " + std::to_string(p.prep_minutes) + " min = base " + std::to_string(base) + " (" +
                          e.event_type + ") + learned bias " + signedInt(bias));
  }

  // travel_minutes = eta(mode, origin, destination) * learned_travel_factor[mode]
  Place to = in.destination.label.empty() ? Place{e.location, std::nullopt, std::nullopt} : in.destination;
  RouteEta eta = router_.eta(p.mode, in.origin, to);
  double factor = in.mode_stats.travel_factor > 0 ? in.mode_stats.travel_factor : 1.0;
  int ride = std::max(1, static_cast<int>(std::lround(eta.minutes * factor)));
  // Transit trips start and end on foot: walk to the stop and from the stop, at the learned walking pace.
  int walk_to = 0, walk_from = 0;
  if (event_types_.contains("mode_legs") && event_types_["mode_legs"].contains(p.mode)) {
    auto& legs = event_types_["mode_legs"][p.mode];
    double wf = in.walk_stats.travel_factor > 0 ? in.walk_stats.travel_factor : 1.0;
    walk_to = static_cast<int>(std::lround(legs.value("walk_to_stop_minutes", 0) * wf));
    walk_from = static_cast<int>(std::lround(legs.value("walk_from_stop_minutes", 0) * wf));
  }
  p.travel_minutes = walk_to + ride + walk_from;
  if (walk_to + walk_from > 0) {
    p.reasoning.push_back("travel " + std::to_string(p.travel_minutes) + " min = walk to stop " + std::to_string(walk_to) +
                          " + " + p.mode + " " + std::to_string(ride) + " (" + std::to_string(eta.minutes) +
                          " min ETA x learned factor " + factor2(factor) + ") + walk from stop " +
                          std::to_string(walk_from));
  } else {
    p.reasoning.push_back("travel " + std::to_string(p.travel_minutes) + " min = " + std::to_string(eta.minutes) +
                          " min " + p.mode + " ETA x learned factor " + factor2(factor));
  }

  // leave_by = event.start - travel_minutes - buffer
  p.buffer_minutes = buffer(e.event_type);
  p.leave_by = e.start_ts - static_cast<long long>(p.travel_minutes + p.buffer_minutes) * 60;
  p.reasoning.push_back("leave_by " + hhmm(p.leave_by) + " = start " + hhmm(e.start_ts) + " - travel " +
                        std::to_string(p.travel_minutes) + " - buffer " + std::to_string(p.buffer_minutes));

  // reminder_at = leave_by - prep_minutes
  p.reminder_at = p.leave_by - static_cast<long long>(p.prep_minutes) * 60;
  p.reasoning.push_back("reminder_at " + hhmm(p.reminder_at) + " = leave_by " + hhmm(p.leave_by) + " - prep " +
                        std::to_string(p.prep_minutes));

  UrgencyInput u;
  u.event = e;
  u.slack_minutes = in.now > 0 ? static_cast<int>((p.reminder_at - in.now) / 60) : 120;
  u.events_today = in.events_today;
  u.has_deadline = hardStart(e.event_type);
  u.bias = in.type_stats;
  UrgencyResult ur = urgency_.score(u);
  p.urgency = ur.level;
  p.urgency_score = ur.score;
  p.reasoning.push_back(ur.reasoning.back());
  return p;
}

}  // namespace enro
