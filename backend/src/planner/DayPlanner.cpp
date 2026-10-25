#include "planner/DayPlanner.hpp"

#include <algorithm>
#include <cctype>
#include <limits>

#include "app/App.hpp"
#include "models/Json.hpp"
#include "util/Time.hpp"

namespace enro {

namespace {
constexpr int kMaxWalkMeters = 3000;  // longer "walks" are planned as bus rides
}  // namespace

namespace {

constexpr long long kDay = 24 * 3600;
const char* kDefaultTz = "America/Los_Angeles";

std::string tzOf(const User& u) { return u.timezone.empty() ? kDefaultTz : u.timezone; }

int stakes(const std::string& type) {
  if (type == "flight") return 3;
  if (type == "interview" || type == "exam") return 2;
  if (type == "class" || type == "meeting") return 1;
  return 0;
}

// Onboarding asks preferred mode per trip type: campus | off_campus | night.
std::string tripType(const Event& e, const std::string& tz) {
  int hour = std::stoi(time::hhmmIn(e.start_ts, tz).substr(0, 2));
  if (hour >= 21 || hour < 5) return "night";
  for (const char* m : {"UC San Diego", "UCSD", "Geisel", "Price Center", "Center Hall", "WLH", "campus"})
    if (e.location.find(m) != std::string::npos) return "campus";
  return "off_campus";
}

}  // namespace

std::string DayPlanner::defaultDate(const std::string& user_id) const {
  auto user = ctx_.repos->user(user_id);
  std::string tz = user ? tzOf(*user) : kDefaultTz;
  long long now = time::nowTs();
  std::string today = time::dateIn(now, tz);
  long long midnight = time::midnightIn(today, tz);
  long long first_future = std::numeric_limits<long long>::max();
  for (auto& e : ctx_.repos->events(user_id, midnight, std::numeric_limits<long long>::max())) {
    if (e.canceled) continue;
    if (e.start_ts < midnight + kDay) return today;
    first_future = std::min(first_future, e.start_ts);
  }
  return first_future == std::numeric_limits<long long>::max() ? today : time::dateIn(first_future, tz);
}

DayPlan DayPlanner::build(const std::string& user_id, const std::string& date_in) {
  DayPlan day;
  day.user_id = user_id;
  auto user = ctx_.repos->user(user_id);
  User u = user ? *user : User{};
  if (!user) u.id = user_id;
  day.timezone = tzOf(u);
  day.date = date_in.empty() ? defaultDate(user_id) : date_in;
  day.generated_at = time::nowTs();
  u.timezone = day.timezone;

  long long midnight = time::midnightIn(day.date, day.timezone);
  day.events = ctx_.repos->events(user_id, midnight, midnight + kDay);
  std::sort(day.events.begin(), day.events.end(), [](auto& a, auto& b) { return a.start_ts < b.start_ts; });

  std::vector<const Event*> live;
  for (auto& e : day.events)
    if (!e.canceled) live.push_back(&e);

  const Event* prev = nullptr;
  for (const Event* e : live) {
    LeaveInput in;
    in.user = u;
    in.event = *e;
    in.events_today = static_cast<int>(live.size());
    in.type_stats = ctx_.punctuality->forEventType(e->event_type);
    if (prev && !ReadyToLeave::hardStart(e->event_type)) {
      // Not the first event: the user is already out, coming from the previous one. Interviews, exams,
      // and flights still get their full prep.
      in.already_out = true;
      in.origin = ctx_.places->resolve(prev->location);
      if (e->start_ts - prev->end_ts <= 30 * 60) in.after_title = prev->title;
    } else {
      in.origin = ctx_.places->resolve(u.home_area);
    }
    std::string trip = tripType(*e, day.timezone);
    auto pref = u.preferred_mode.find(trip);
    std::string preferred = pref != u.preferred_mode.end() ? pref->second : "walk";
    in.destination = ctx_.places->resolve(e->location);  // coordinates from backend/config/places.json
    int meters = FallbackRouter::distanceMeters(in.origin, in.destination);
    std::string predicted = ctx_.transport->predict(e->event_type, meters, preferred);
    in.mode = predicted.empty() ? preferred : predicted;
    // Habits learned on campus don't apply to long trips: nobody walks back from Sorrento Valley.
    if (in.mode == "walk" && meters > kMaxWalkMeters) in.mode = "bus";
    in.mode_stats = ctx_.punctuality->forMode(in.mode);
    in.walk_stats = ctx_.punctuality->forMode("walk");
    day.plans.push_back(ctx_.ready_to_leave->plan(in));
    prev = e;
  }

  day.wake = ctx_.wake_ladder->plan(u, day.date);

  auto hhmm = [&](long long ts) { return time::hhmmIn(ts, day.timezone); };
  if (live.empty()) {
    day.reasoning.push_back("No events on " + day.date + ", nothing to plan");
  } else {
    day.reasoning.push_back(std::to_string(live.size()) + " event" + (live.size() == 1 ? "" : "s") + " today, first at " +
                            hhmm(live.front()->start_ts) + " (" + live.front()->title + ")");
    size_t top = 0;
    for (size_t i = 1; i < live.size(); ++i) {
      bool higher = stakes(live[i]->event_type) > stakes(live[top]->event_type) ||
                    (stakes(live[i]->event_type) == stakes(live[top]->event_type) &&
                     day.plans[i].urgency_score > day.plans[top].urgency_score);
      if (higher) top = i;
    }
    std::string type = live[top]->event_type;
    if (!type.empty()) type[0] = static_cast<char>(std::toupper(type[0]));
    day.reasoning.push_back(type + " at " + hhmm(live[top]->start_ts) + " is the highest-stakes event (" +
                            nlohmann::json(day.plans[top].urgency).get<std::string>() + ", " +
                            std::to_string(day.plans[top].urgency_score) + ")");
  }
  if (!day.wake.alarm_time.empty()) {
    day.reasoning.push_back("Alarm " + day.wake.alarm_time + " + " + std::to_string(day.wake.grace_minutes) +
                            " min grace -> first wake call " + hhmm(day.wake.alarm_ts + day.wake.grace_minutes * 60LL));
  }
  return day;
}

ConsequenceReport DayPlanner::consequences(const std::string& user_id, int delay_minutes, const std::string& date) {
  DayPlan day = build(user_id, date);
  return ctx_.consequences->evaluate(user_id, day.events, day.plans, delay_minutes);
}

}  // namespace enro
