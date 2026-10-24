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
  // {event, minutes, consequences, history, ...}. Set has_contacts=true only when the user has a
  // confirmed emergency contact; it is the one thing a line may warn about (PRD 7.10).
  nlohmann::json facts = nlohmann::json::object();
};

class LineGenerator {
 public:
  LineGenerator(LlmClient& llm, nlohmann::json personas, nlohmann::json scripts);
  std::string generate(const LineRequest& req);

  // First canned line for `purpose` from scripts.json with {name}, {event}, {minutes} filled in.
  // Tries `<purpose>_<urgency>` (e.g. leave_reminder_urgent) before `purpose`. Empty if neither exists.
  std::string canned(const LineRequest& req) const;

  // Persona + urgency style + length and safety rules, sent as the system message.
  std::string systemPrompt(const LineRequest& req) const;

  // Spoken lines get shorter as urgency rises.
  static int maxWords(Urgency urgency);

  // False if the line threatens anything, or mentions emergency contacts when there are none.
  static bool isSafe(const std::string& line, bool has_contacts);

 private:
  const nlohmann::json* archetype(const std::string& id) const;
  std::string fallback(const LineRequest& req) const;

  LlmClient& llm_;
  nlohmann::json personas_, scripts_;
};

}  // namespace enro
