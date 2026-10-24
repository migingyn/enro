// Turn-based call conversation (PRD 6 P0): classify what the user said, negotiate snoozes with
// real numbers from the Consequence Engine and punctuality model, and pick the next line.
// Owner: T11. Every turn is recorded and broadcast as a `turn` dashboard event.
#pragma once

#include <optional>
#include <string>

#include "models/Models.hpp"

namespace enro {

struct AppContext;

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

 private:
  AgentLine say(const Call& call, const std::string& purpose, const nlohmann::json& facts);
  AppContext& ctx_;
};

}  // namespace enro
