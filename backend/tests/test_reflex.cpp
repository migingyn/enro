// T14: Reflex (RulesReflex + JevReflex).
#include <doctest/doctest.h>

#include "reflex/JevReflex.hpp"

using namespace enro;

TEST_CASE("every decision returns one of its allowed verdicts") {
  RulesReflex r;
  auto in = nlohmann::json::object();
  auto one_of = [](const std::string& v, std::initializer_list<const char*> ok) {
    for (auto* o : ok)
      if (v == o) return true;
    return false;
  };
  CHECK(one_of(r.emailRelevance(in).verdict, {"IGNORE", "MAYBE", "PLAN_CHANGE"}));
  CHECK(one_of(r.transitSpeak(in).verdict, {"IGNORE", "COMMENT", "ALERT", "REROUTE"}));
  CHECK(one_of(r.interruptChannel(in).verdict, {"WAIT", "EARBUD", "CALL"}));
  CHECK(one_of(r.readiness(in).verdict, {"READY", "STALLING", "UNCLEAR"}));
  CHECK(one_of(r.snooze(in).verdict, {"GRANT", "COUNTER", "REFUSE"}));
  CHECK(one_of(r.stopCondition(in).verdict, {"CONTINUE", "BACK_OFF", "STOP", "ESCALATE"}));
}

TEST_CASE("JevReflex without a key falls back to rules") {
  JevReflex j(Env::fromMap({}));
  CHECK(j.emailRelevance({}).source == "rules");
}

TEST_CASE("TODO(T14): Jev timeout falls back to rules within JEV_TIMEOUT_MS" * doctest::skip()) {}
TEST_CASE("TODO(T14): Jev verdicts carry source jev and latency_ms" * doctest::skip()) {}
TEST_CASE("TODO(T14): rules: 'canceled' in subject from a course address -> PLAN_CHANGE" * doctest::skip()) {}
TEST_CASE("TODO(T14): rules: off-route tick -> REROUTE" * doctest::skip()) {}
