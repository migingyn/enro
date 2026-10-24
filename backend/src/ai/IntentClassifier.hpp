// Classifies what the user said on a call: SNOOZE_REQUEST (with minutes), AWAKE, READY, STALLING,
// LEFT, ASK_CONSEQUENCES, UNCLEAR. LLM-backed with a keyword fallback. Owner: T11.
#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "ai/LlmClient.hpp"

namespace enro {

struct Intent {
  std::string label = "UNCLEAR";
  int minutes = 0;  // for SNOOZE_REQUEST: "five more minutes" -> 5
  double confidence = 0.0;
};

class IntentClassifier {
 public:
  explicit IntentClassifier(LlmClient& llm) : llm_(llm) {}
  // context: {purpose, attempt, last_agent_line}
  Intent classify(const std::string& text, const nlohmann::json& context = {});
  // Keyword rules. Used as is with the fake LLM, and as the fallback when the LLM fails.
  static Intent keywords(const std::string& text);

 private:
  LlmClient& llm_;
};

}  // namespace enro
