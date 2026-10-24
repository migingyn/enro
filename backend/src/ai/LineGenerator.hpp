// Writes every spoken line (PRD 7.11: the reflex decides whether to speak, the LLM writes what).
// Persona prompt from backend/config/personas.json, tone shifted by urgency, real numbers from the
// Consequence Engine and punctuality model in `facts`. Falls back to backend/config/scripts.json
// with no LLM key. Owner: T12.
#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "ai/LlmClient.hpp"
#include "models/Models.hpp"

namespace enro {

struct LineRequest {
  std::string purpose;  // a key in scripts.json, e.g. wake_call_1, snooze_counter, transit_reroute
  std::string persona;  // archetype id
  Urgency urgency = Urgency::CHILL;
  std::string user_name;
  nlohmann::json facts = nlohmann::json::object();  // {event, minutes, consequences, history, ...}
};

class LineGenerator {
 public:
  LineGenerator(LlmClient& llm, nlohmann::json personas, nlohmann::json scripts);
  std::string generate(const LineRequest& req);

  // First canned line for `purpose` from scripts.json with {name}, {event}, {minutes} filled in.
  // Empty if scripts.json has no such key.
  std::string canned(const LineRequest& req) const;

 private:
  LlmClient& llm_;
  nlohmann::json personas_, scripts_;
};

}  // namespace enro
