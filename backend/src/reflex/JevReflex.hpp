// Jev reflex over HTTP (TypeSafe API, or OpenRouter as a second route). Selected by
// REFLEX_PROVIDER=jev. Falls back to RulesReflex on timeout or missing key. Owner: T14.
// Verify the current API shape and model name before wiring it (PRD 7.11).
#pragma once

#include <memory>
#include <optional>

#include "config/Env.hpp"
#include "reflex/Reflex.hpp"
#include "reflex/RulesReflex.hpp"

namespace enro {

class JevReflex : public Reflex {
 public:
  explicit JevReflex(const Env& env);

  std::string name() const override { return "jev"; }

  ReflexVerdict emailRelevance(const nlohmann::json& email) override;
  ReflexVerdict transitSpeak(const nlohmann::json& tick) override;
  ReflexVerdict interruptChannel(const nlohmann::json& context) override;
  ReflexVerdict readiness(const nlohmann::json& reply) override;
  ReflexVerdict snooze(const nlohmann::json& request) override;
  ReflexVerdict stopCondition(const nlohmann::json& state) override;

 private:
  // One HTTP call: decision name + allowed verdicts + input. nullopt on timeout or error.
  std::optional<ReflexVerdict> ask(const std::string& decision, const nlohmann::json& verdicts,
                                   const nlohmann::json& input);

  std::string api_key_, model_, openrouter_key_;
  int timeout_ms_ = 400;
  RulesReflex fallback_;
};

}  // namespace enro
