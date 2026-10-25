// Joins a wake call to phone-motion evidence (S7). A phone call can't read the phone's accelerometer,
// so Sensor Logger and the voice provider are separate channels joined here by time: only movement
// after the call was answered counts.
//
// The result is evidence for the dashboard, not a gate: "I'm up" still ends the call either way.
// Movement is a heuristic (see MotionTracker.hpp), so it never overrules what the user said.
#pragma once

#include <string>

#include "models/Models.hpp"
#include "motion/MotionTracker.hpp"

namespace enro {

struct AppContext;

// Call when the call is answered. Starts watching for movement after this moment.
void noteCallAnswered(AppContext& ctx, const std::string& call_id);

// Call when the user says they're up. Writes a `wake_evidence` Decision (source "rules",
// verdict moving | stationary | stale | unavailable), broadcasts it, and stops watching the call.
// Returns the verdict.
std::string recordWakeEvidence(AppContext& ctx, const Call& call);

}  // namespace enro
