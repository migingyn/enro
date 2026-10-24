#include "planner/ConsequenceEngine.hpp"

#include <algorithm>
#include <map>

namespace enro {

namespace {

// Showing up late to these is the same as missing them, so the MISSED line is much closer.
bool hardStart(const std::string& type) { return type == "interview" || type == "exam" || type == "flight"; }

int missedAfterMinutes(const Event& e) {
  if (hardStart(e.event_type)) return 15;
  int duration = static_cast<int>((e.end_ts - e.start_ts) / 60);
  return std::max(10, duration / 2);
}

std::string minutes(int n) { return std::to_string(n) + " minute" + (n == 1 ? "" : "s"); }

}  // namespace

ConsequenceReport ConsequenceEngine::evaluate(const std::string& user_id, const std::vector<Event>& events,
                                              const std::vector<LeavePlan>& plans, int delay_minutes) const {
  ConsequenceReport r;
  r.user_id = user_id;
  r.delay_minutes = delay_minutes;
  r.worst = ConsequenceStatus::OK;

  std::map<std::string, const LeavePlan*> plan_for;
  for (auto& p : plans) plan_for[p.event_id] = &p;

  std::vector<const Event*> day;
  for (auto& e : events)
    if (!e.canceled) day.push_back(&e);
  std::sort(day.begin(), day.end(), [](auto* a, auto* b) { return a->start_ts < b->start_ts; });

  // The delay hits the first event. A later event only feels it if the user would still be on the way to
  // the previous one when it's time to leave for it; then the overlap carries forward.
  int carry = std::max(0, delay_minutes);
  long long prev_arrival = 0;
  for (size_t i = 0; i < day.size(); ++i) {
    const Event& e = *day[i];
    auto it = plan_for.find(e.id);
    int buffer = it != plan_for.end() ? it->second->buffer_minutes : 5;
    long long leave_by = it != plan_for.end() ? it->second->leave_by : e.start_ts - buffer * 60LL;

    if (i > 0) carry = std::min(carry, static_cast<int>(std::max(0LL, prev_arrival - leave_by) / 60));

    int late = std::max(0, carry - buffer);
    ConsequenceStatus status = late == 0                      ? ConsequenceStatus::OK
                               : late >= missedAfterMinutes(e) ? ConsequenceStatus::MISSED
                                                               : ConsequenceStatus::AT_RISK;
    r.items.push_back({e.id, e.title, e.start_ts, status, late});
    r.worst = std::max(r.worst, status);
    // Leaving `carry` minutes behind plan means arriving at start - buffer + carry.
    prev_arrival = e.start_ts + (carry - buffer) * 60LL;
  }

  std::string lead = std::to_string(delay_minutes) + " more minute" + (delay_minutes == 1 ? "" : "s");
  auto hit = std::find_if(r.items.begin(), r.items.end(), [](auto& i) { return i.minutes_late > 0; });
  if (hit == r.items.end()) {
    r.summary = lead + " is fine. You're still on time for everything.";
    return r;
  }
  r.summary = hit->status == ConsequenceStatus::MISSED ? lead + " means you miss " + hit->title + "."
                                                        : lead + " makes you " + minutes(hit->minutes_late) +
                                                              " late to " + hit->title + ".";
  // Then say what happens to the highest-stakes event, if it isn't the one already named.
  for (size_t i = 0; i < day.size(); ++i) {
    if (!hardStart(day[i]->event_type) || r.items[i].event_id == hit->event_id) continue;
    const char* fate = r.items[i].status == ConsequenceStatus::OK       ? " is still safe."
                       : r.items[i].status == ConsequenceStatus::MISSED ? " is missed too."
                                                                        : " is at risk too.";
    r.summary += " The " + day[i]->event_type + fate;
    break;
  }
  return r;
}

}  // namespace enro
