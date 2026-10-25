// T09: Email triage (PRD 7.2).
#include <doctest/doctest.h>

#include "TestApp.hpp"
#include "ai/FakeLlm.hpp"
#include "reflex/RulesReflex.hpp"
#include "store/Seed.hpp"

using namespace enro;

namespace {

// Reflex with a fixed email verdict, so these tests don't depend on T14's rules.
struct FixedReflex : RulesReflex {
  std::string email_verdict;
  explicit FixedReflex(std::string v) : email_verdict(std::move(v)) {}
  ReflexVerdict emailRelevance(const nlohmann::json&) override { return {email_verdict, 0.9}; }
};

// LLM that counts calls and returns a scripted extraction (or the FakeLlm default).
struct ScriptedLlm : FakeLlm {
  int calls = 0;
  std::optional<nlohmann::json> answer;
  nlohmann::json chatJson(const std::string& s, const std::string& u, const nlohmann::json& schema) override {
    ++calls;
    return answer ? *answer : FakeLlm::chatJson(s, u, schema);
  }
};

struct Triage {
  std::unique_ptr<AppContext> app = test::fakeApp();
  std::shared_ptr<ScriptedLlm> llm = std::make_shared<ScriptedLlm>();
  explicit Triage(const std::string& verdict) {
    seedDemo(*app);
    app->reflex = std::make_unique<FixedReflex>(verdict);
    app->llm = llm;
  }
};

EmailMessage canceledSection() {
  return {"msg_1003", "cse110-tas@ucsd.edu", "Section canceled today",
          "Hi all, discussion section is canceled today. See you Wednesday.", 0};
}

}  // namespace

TEST_CASE("process writes one email_relevance decision per message") {
  auto app = test::fakeApp();
  auto out = app->triage->process("u_demo", {{"m1", "a@b.c", "Hello", "hi", 0}, {"m2", "x@y.z", "Promo", "sale", 0}});
  CHECK(out.size() == 2);
  CHECK(app->repos->recentDecisions().size() == 2);
}

TEST_CASE("'Section canceled today' -> PLAN_CHANGE cancels evt_cse110_dis and the plan shifts") {
  Triage t("PLAN_CHANGE");
  auto out = t.app->triage->process("u_demo", {canceledSection()});
  REQUIRE(out.size() == 1);
  REQUIRE(out[0].extracted);
  CHECK((*out[0].extracted)["event_id"] == "evt_cse110_dis");
  CHECK((*out[0].extracted)["canceled"] == true);
  CHECK(t.app->repos->event("evt_cse110_dis")->canceled);

  // The rebuilt plan was broadcast: discussion canceled, no leave plan for it.
  auto events = t.app->dashboard->recent();
  auto plan = std::find_if(events.rbegin(), events.rend(), [](auto& m) { return m["type"] == "plan"; });
  REQUIRE(plan != events.rend());
  bool has_dis_plan = false;
  for (auto& p : (*plan)["data"]["plans"]) has_dis_plan |= p["event_id"] == "evt_cse110_dis";
  CHECK_FALSE(has_dis_plan);
}

TEST_CASE("IGNORE and MAYBE emails never reach the LLM") {
  Triage ignore("IGNORE");
  ignore.app->triage->process("u_demo", {{"m1", "deals@shop.com", "50% off", "sale", 0}});
  CHECK(ignore.llm->calls == 0);
  Triage maybe("MAYBE");
  maybe.app->triage->process("u_demo", {{"m2", "prof@ucsd.edu", "Office hours", "see you", 0}});
  CHECK(maybe.llm->calls == 0);
  CHECK_FALSE(maybe.app->repos->event("evt_cse110_dis")->canceled);
}

TEST_CASE("the same message id is not parsed twice") {
  Triage t("PLAN_CHANGE");
  t.app->triage->process("u_demo", {canceledSection()});
  int calls = t.llm->calls;
  auto decisions = t.app->repos->recentDecisions().size();
  auto again = t.app->triage->process("u_demo", {canceledSection()});
  CHECK(again.size() == 1);
  CHECK(t.llm->calls == calls);
  CHECK(t.app->repos->recentDecisions().size() == decisions);
}

TEST_CASE("only the extracted fact is stored, never the body") {
  Triage t("PLAN_CHANGE");
  t.app->triage->process("u_demo", {canceledSection()});
  auto facts = t.app->repos->emailFacts("u_demo");
  REQUIRE(facts.size() == 1);
  CHECK(facts[0].message_id == "msg_1003");
  CHECK(facts[0].verdict == "PLAN_CHANGE");
  CHECK_FALSE(facts[0].extracted.contains("snippet"));
  CHECK_FALSE(facts[0].extracted.contains("body"));
}

TEST_CASE("an LLM extraction can move an event to a new time and place") {
  Triage t("PLAN_CHANGE");
  long long new_start = t.app->repos->event("evt_lunch")->start_ts + 3600;
  t.llm->answer = nlohmann::json{{"event_id", "evt_lunch"}, {"canceled", false}, {"new_start_ts", new_start},
                                 {"new_location", "Sixty-Four Degrees"}};
  t.app->triage->process("u_demo", {{"m9", "alex@ucsd.edu", "Lunch moved to 2", "can we do 2pm instead?", 0}});
  auto e = t.app->repos->event("evt_lunch");
  CHECK(e->start_ts == new_start);
  CHECK(e->location == "Sixty-Four Degrees");
  CHECK(e->end_ts - e->start_ts == 3600);  // duration kept
}

TEST_CASE("an extraction naming an unknown event changes nothing") {
  Triage t("PLAN_CHANGE");
  t.llm->answer = nlohmann::json{{"event_id", "evt_nope"}, {"canceled", true}, {"new_start_ts", nullptr},
                                 {"new_location", nullptr}};
  auto out = t.app->triage->process("u_demo", {{"m8", "x@y.z", "Canceled", "the thing is canceled", 0}});
  CHECK_FALSE(out[0].extracted);
  CHECK_FALSE(t.app->repos->event("evt_cse110_dis")->canceled);
}

TEST_CASE("an email whose event is already canceled never cancels a different event") {
  Triage t("PLAN_CHANGE");
  t.app->triage->process("u_demo", {canceledSection()});
  REQUIRE(t.app->repos->event("evt_cse110_dis")->canceled);
  // The same email again, with a fresh triage instance (as after a restart): the lecture must survive.
  EmailTriage fresh(*t.app);
  auto again = canceledSection();
  again.id = "msg_1003_resent";
  fresh.process("u_demo", {again});
  CHECK(t.app->repos->event("evt_cse110_dis")->canceled);
  CHECK_FALSE(t.app->repos->event("evt_cse110_lec")->canceled);
}

TEST_CASE("a message triaged before a restart is not triaged again") {
  Triage t("PLAN_CHANGE");
  t.app->triage->process("u_demo", {canceledSection()});
  int calls = t.llm->calls;
  auto decisions = t.app->repos->recentDecisions(500).size();
  EmailTriage after_restart(*t.app);  // empty in-memory cache, same database
  after_restart.process("u_demo", {canceledSection()});
  CHECK(t.llm->calls == calls);
  CHECK(t.app->repos->recentDecisions(500).size() == decisions);
}
