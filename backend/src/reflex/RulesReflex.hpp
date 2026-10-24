// Rules-based reflex. Default provider, and the fallback JevReflex uses on timeout or missing key.
#pragma once

#include "reflex/Reflex.hpp"

namespace enro {

class RulesReflex : public Reflex {
 public:
  std::string name() const override { return "rules"; }

  ReflexVerdict emailRelevance(const nlohmann::json& email) override;
  ReflexVerdict transitSpeak(const nlohmann::json& tick) override;
  ReflexVerdict interruptChannel(const nlohmann::json& context) override;
  ReflexVerdict readiness(const nlohmann::json& reply) override;
  ReflexVerdict snooze(const nlohmann::json& request) override;
  ReflexVerdict stopCondition(const nlohmann::json& state) override;
};

}  // namespace enro
