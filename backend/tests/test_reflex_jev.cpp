// T14: JevReflex. Network is replaced by a fake transport; a live check is in scripts/check_jev.py.
#include <doctest/doctest.h>

#include "reflex/JevReflex.hpp"

using namespace enro;
using nlohmann::json;

namespace {
Env jevEnv() { return Env::fromMap({{"TYPESAFE_API_KEY", "test-key"}, {"JEV_MODEL", "jev-test"}}); }

// Transport that answers every noul with the probability in `nouls` and records the last request.
struct FakeJev {
  std::map<std::string, double> nouls;
  json last_request;
  bool fail = false;
  JevReflex::Transport fn() {
    return [this](const json& req) -> std::optional<json> {
      last_request = req;
      if (fail) return std::nullopt;
      json answers = json::object();
      for (auto& [name, q] : req["questions"].items()) answers[name] = {{"type", "noul"}, {"noul", nouls.count(name) ? nouls[name] : 0.1}};
      return json{{"model", req["model"]}, {"answers", answers}, {"usage", {{"input_tokens", 10}, {"output_tokens", 2}}}};
    };
  }
};
}  // namespace

TEST_CASE("plan-changing email -> PLAN_CHANGE from Jev, with the request carrying only metadata + snippet") {
  FakeJev fake;
  fake.nouls = {{"changes_plan", 0.93}, {"about_schedule", 0.97}};
  JevReflex jev(jevEnv(), fake.fn());
  auto v = jev.emailRelevance({{"message_id", "m1"}, {"from", "cse110-tas@ucsd.edu"}, {"subject", "Section canceled today"},
                               {"snippet", "discussion section is canceled today"}});
  CHECK(v.verdict == "PLAN_CHANGE");
  CHECK(v.source == "jev");
  CHECK(v.confidence == doctest::Approx(0.93));
  CHECK(fake.last_request["model"] == "jev-test");
  CHECK(fake.last_request["state"]["email"]["subject"] == "Section canceled today");
  CHECK_FALSE(fake.last_request["state"]["email"].contains("message_id"));  // ids mean nothing to the model
}

TEST_CASE("schedule-related but not a change -> MAYBE; neither -> IGNORE") {
  FakeJev fake;
  fake.nouls = {{"changes_plan", 0.1}, {"about_schedule", 0.8}};
  JevReflex jev(jevEnv(), fake.fn());
  CHECK(jev.emailRelevance({{"subject", "Office hours this week"}}).verdict == "MAYBE");
  fake.nouls = {{"changes_plan", 0.05}, {"about_schedule", 0.1}};
  CHECK(jev.emailRelevance({{"subject", "20% off hoodies"}}).verdict == "IGNORE");
}

TEST_CASE("readiness: ready vs stalling vs unclear from the reply text") {
  FakeJev fake;
  JevReflex jev(jevEnv(), fake.fn());
  fake.nouls = {{"getting_up", 0.9}, {"asking_for_time", 0.05}};
  CHECK(jev.readiness({{"text", "I'm up, I'm up, getting dressed"}}).verdict == "READY");
  fake.nouls = {{"getting_up", 0.2}, {"asking_for_time", 0.92}};
  CHECK(jev.readiness({{"text", "give me five more minutes"}}).verdict == "STALLING");
  fake.nouls = {{"getting_up", 0.4}, {"asking_for_time", 0.4}};
  CHECK(jev.readiness({{"text", "mmph"}}).verdict == "UNCLEAR");
}

TEST_CASE("timeout or error falls back to rules, never to a guessed answer") {
  FakeJev fake;
  fake.fail = true;
  JevReflex jev(jevEnv(), fake.fn());
  auto v = jev.emailRelevance({{"from", "cse110-tas@ucsd.edu"}, {"subject", "Section canceled today"}, {"snippet", "canceled"}});
  CHECK(v.source == "rules");
  CHECK(v.verdict == RulesReflex().emailRelevance({{"from", "cse110-tas@ucsd.edu"}, {"subject", "Section canceled today"},
                                                   {"snippet", "canceled"}}).verdict);
}

TEST_CASE("numeric decisions stay on rules (no Jev call)") {
  FakeJev fake;
  JevReflex jev(jevEnv(), fake.fn());
  json tick = {{"on_route", false}, {"delay_minutes", 7}};
  CHECK(jev.transitSpeak(tick).verdict == "REROUTE");
  CHECK(jev.stopCondition({{"attempt", 3}}).source == "rules");
  CHECK(fake.last_request.is_null());
}

TEST_CASE("no key -> rules without trying the network") {
  FakeJev fake;
  JevReflex jev(Env::fromMap({}), fake.fn());
  CHECK(jev.emailRelevance({{"subject", "x"}}).source == "rules");
  CHECK(fake.last_request.is_null());
}
