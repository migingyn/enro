#include "planner/ConsequenceEngine.hpp"

namespace enro {

ConsequenceReport ConsequenceEngine::evaluate(const std::string& user_id, const std::vector<Event>& events,
                                              const std::vector<LeavePlan>&, int delay_minutes) const {
  // STUB
  // TODO(T08): shift each leave_by by delay_minutes, compare against buffer: minutes_late > 0 -> AT_RISK,
  // past a threshold or event already started -> MISSED; propagate to back-to-back events; set worst + summary.
  ConsequenceReport r;
  r.user_id = user_id;
  r.delay_minutes = delay_minutes;
  r.worst = ConsequenceStatus::OK;
  for (auto& e : events) {
    if (e.canceled) continue;
    r.items.push_back({e.id, e.title, e.start_ts, ConsequenceStatus::OK, 0});
  }
  r.summary = "[stub] everything is still on time";
  return r;
}

}  // namespace enro
