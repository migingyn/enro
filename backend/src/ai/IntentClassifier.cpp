#include "ai/IntentClassifier.hpp"

namespace enro {

Intent IntentClassifier::classify(const std::string&, const nlohmann::json&) {
  // STUB
  // TODO(T11): llm_.chatJson with a {label, minutes, confidence} schema; keyword fallback for the fake LLM
  // ("five more minutes" -> SNOOZE_REQUEST 5, "I'm up" -> AWAKE, "leaving now" -> LEFT).
  (void)llm_;
  return {"UNCLEAR", 0, 0.5};
}

}  // namespace enro
