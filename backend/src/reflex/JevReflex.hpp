// Jev reflex over the TypeSafe System One API (REFLEX_PROVIDER=jev). Owner: T14.
//
// Jev makes judgments, code makes decisions: language questions (does this email change a plan? is this
// reply stalling?) go to Jev as atomic Noul questions, and fixed thresholds in JevReflex.cpp turn the
// probabilities into verdicts. Decisions that are arithmetic on known numbers (transit delay vs slack,
// interrupt channel, snooze vs consequences, ladder stop) stay in RulesReflex, where code is reliable.
// Any failure (no key, timeout, HTTP error, bad answer) falls back to RulesReflex, never to a guess.
#pragma once

#include <functional>
#include <optional>

#include "config/Env.hpp"
#include "reflex/Reflex.hpp"
#include "reflex/RulesReflex.hpp"

namespace enro {

class JevReflex : public Reflex {
 public:
  // POST /v1/systemone body -> response body, or nullopt on timeout or error. Tests inject a fake.
  using Transport = std::function<std::optional<nlohmann::json>(const nlohmann::json& request)>;

  explicit JevReflex(const Env& env, Transport transport = nullptr);

  std::string name() const override { return "jev"; }

  ReflexVerdict emailRelevance(const nlohmann::json& email) override;
  ReflexVerdict transitSpeak(const nlohmann::json& tick) override;
  ReflexVerdict interruptChannel(const nlohmann::json& context) override;
  ReflexVerdict readiness(const nlohmann::json& reply) override;
  ReflexVerdict snooze(const nlohmann::json& request) override;
  ReflexVerdict stopCondition(const nlohmann::json& state) override;

 private:
  // One request; returns {question name -> noul probability} plus latency, or nullopt.
  struct NoulAnswers {
    std::map<std::string, double> p;
    int latency_ms = 0;
  };
  std::optional<NoulAnswers> askNouls(const nlohmann::json& state, const nlohmann::json& questions);

  std::string api_key_, model_, base_url_;
  int timeout_ms_ = 800;
  Transport transport_;
  RulesReflex fallback_;
};

}  // namespace enro
