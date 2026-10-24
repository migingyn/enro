#include "sim/LoadSimulator.hpp"

#include "util/Time.hpp"

namespace enro {

SimResult LoadSimulator::run(int users, int threads) {
  // STUB
  // TODO(T16): generate synthetic users (wake times, event mixes), shard by wake minute across `threads`,
  // run ReadyToLeave + Urgency per event, count scheduled tasks/calls per minute, apply rates_ for cost.
  SimResult r;
  r.run_id = "sim_" + std::to_string(time::nowMs());
  r.users = users;
  r.threads = threads;
  r.runtime_ms = 0;
  r.cost_breakdown = {{"llm", 0.0}, {"tts", 0.0}, {"telephony", 0.0}, {"reflex", 0.0}};
  return r;
}

}  // namespace enro
