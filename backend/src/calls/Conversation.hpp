// Turn-based call conversation (PRD 6 P0): classify what the user said, negotiate snoozes with
// real numbers from the Consequence Engine and punctuality model, and pick the next line.
// Owner: T11. Every turn is recorded and broadcast as a `turn` dashboard event.
#pragma once

#include <map>
#include <optional>
#include <string>

#include "models/Models.hpp"

namespace enro {

struct AppContext;

// Snooze negotiation outcome (PRD 3.2): checked against the Consequence Engine and the user's history.
struct SnoozeDecision {
  std::string verdict;      // GRANT | COUNTER | REFUSE
  int requested = 0;        // what they asked for
  int effective = 0;        // what it really turns into: requested x snooze_inflation
  int counter = 0;          // for COUNTER: the most we can give that stays on time (0 = none)
  double inflation = 1.0;
  int history_samples = 0;
  bool repeat_request = false;  // the last three snoozes asked for the same minutes
  ConsequenceReport consequences;
};

struct AgentLine {
  std::string text;
  std::string persona;
  Urgency urgency = Urgency::CHILL;
  bool end_call = false;
};

class Conversation {
 public:
  explicit Conversation(AppContext& ctx) : ctx_(ctx) {}

  // event: answered | speech | declined | no_answer | completed (see VoiceProvider.hpp).
  // Returns the agent's next line, or nullopt when there is nothing to say.
  std::optional<AgentLine> onEvent(const std::string& call_id, const std::string& event, const std::string& text);

  // Evaluates a snooze request for user against today's plan. Writes a `snooze` Decision.
  SnoozeDecision negotiate(const std::string& user_id, int requested_minutes);

 private:
  // fallback is spoken when scripts.json has no line for purpose (keyless fake mode keeps the numbers).
  AgentLine say(const Call& call, const std::string& purpose, const nlohmann::json& facts, const std::string& fallback);
  AppContext& ctx_;
  std::map<std::string, std::string> stage_;  // call_id -> "opened" | "awake_check"
};

}  // namespace enro
