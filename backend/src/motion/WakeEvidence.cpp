#include "motion/WakeEvidence.hpp"

#include "app/App.hpp"
#include "util/Time.hpp"

namespace enro {

void noteCallAnswered(AppContext& ctx, const std::string& call_id) {
  ctx.motion->markAnswered(call_id, static_cast<double>(time::nowMs()));
}

std::string recordWakeEvidence(AppContext& ctx, const Call& call) {
  double now = static_cast<double>(time::nowMs());
  auto answered = ctx.motion->answeredAt(call.id);
  // Unknown answer time: count nothing from before now rather than risk pre-call movement.
  MotionSnapshot snap = ctx.motion->snapshot(now, answered.value_or(now));
  bool observed = ctx.motion->movementObserved(call.id) || snap.status == "moving";
  std::string verdict = observed ? "moving" : snap.status;
  ctx.motion->forgetCall(call.id);

  // The movement detector is an unvalidated heuristic, so its moving/stationary calls get low
  // confidence. "No data" (stale/unavailable) is a plain fact.
  double confidence = verdict == "moving" || verdict == "stationary" ? 0.5 : 1.0;
  nlohmann::json inputs = {{"call_id", call.id},
                           {"purpose", call.purpose},
                           {"attempt", call.attempt},
                           {"answered_at_ms", answered ? nlohmann::json(*answered) : nlohmann::json(nullptr)},
                           {"movement_observed", observed},
                           {"motion", toJson(snap)}};
  ctx.recordDecision(call.user_id, "wake_evidence", {verdict, confidence, "rules", 0}, inputs);
  return verdict;
}

}  // namespace enro
