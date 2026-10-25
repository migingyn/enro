// T14: Reflex (RulesReflex + JevReflex).
#include <chrono>

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

TEST_CASE("Jev timeout falls back to rules within JEV_TIMEOUT_MS") {
  // 10.255.255.1 is unroutable, so the connect hangs until the timeout.
  JevReflex j(Env::fromMap({{"TYPESAFE_API_KEY", "test"}, {"JEV_BASE_URL", "http://10.255.255.1"}, {"JEV_TIMEOUT_MS", "150"}}));
  auto start = std::chrono::steady_clock::now();
  auto v = j.snooze({{"requested_minutes", 5}});
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
  CHECK(v.source == "rules");
  CHECK(ms < 1000);
}

TEST_CASE("Jev verdicts carry source jev and confidence") {
  auto verdicts = nlohmann::json::array({"GRANT", "COUNTER", "REFUSE"});
  auto typesafe = JevReflex::parseVerdict({{"answers", {{{"choice", "COUNTER"}, {"probability", 0.82}}}}}, verdicts);
  REQUIRE(typesafe);
  CHECK(typesafe->verdict == "COUNTER");
  CHECK(typesafe->source == "jev");
  CHECK(typesafe->confidence == doctest::Approx(0.82));

  nlohmann::json chat = {{"choices", {{{"message", {{"content", R"({"verdict":"GRANT","confidence":0.6})"}}}}}}};
  auto router = JevReflex::parseVerdict(chat, verdicts);
  REQUIRE(router);
  CHECK(router->verdict == "GRANT");

  CHECK_FALSE(JevReflex::parseVerdict({{"choice", "MAYBE"}}, verdicts));
}
TEST_CASE("rules: 'canceled' in subject from a course address -> PLAN_CHANGE") {
  RulesReflex r;
  auto v = r.emailRelevance({{"message_id", "msg_1003"},
                             {"from", "cse110-tas@ucsd.edu"},
                             {"subject", "Section canceled today"},
                             {"snippet", "No discussion section today, see you Wednesday."}});
  CHECK(v.verdict == "PLAN_CHANGE");
  CHECK(v.confidence >= 0.9);
  CHECK(v.source == "rules");
  CHECK(r.emailRelevance({{"from", "recruiting@qualcomm.com"}, {"subject", "Interview moved to 11:30"}}).verdict ==
        "PLAN_CHANGE");
}

TEST_CASE("rules: newsletters and promos -> IGNORE, ambiguous -> MAYBE") {
  RulesReflex r;
  CHECK(r.emailRelevance({{"from", "deals@shop.com"}, {"subject", "40% off everything"}, {"snippet", "Unsubscribe"}})
            .verdict == "IGNORE");
  CHECK(r.emailRelevance({{"from", "newsletter@ucsd.edu"}, {"subject", "This week at UCSD"}, {"snippet", "Unsubscribe"}})
            .verdict == "IGNORE");
  // A sale that is "canceled" is still a promo.
  CHECK(r.emailRelevance({{"from", "promo@store.com"}, {"subject", "Flash sale canceled, new deal inside"}}).verdict ==
        "IGNORE");
  CHECK(r.emailRelevance({{"from", "jules@gmail.com"}, {"subject", "Lunch tomorrow?"}}).verdict == "MAYBE");
  CHECK(r.emailRelevance({{"from", "prof@ucsd.edu"}, {"subject", "Office hours"}}).verdict == "MAYBE");
  CHECK(r.emailRelevance({{"from", "friend@gmail.com"}, {"subject", "lol look at this"}}).verdict == "IGNORE");
}

TEST_CASE("rules: off-route tick -> REROUTE") {
  RulesReflex r;
  CHECK(r.transitSpeak({{"seq", 17}, {"on_route", false}, {"delay_minutes", 7}}).verdict == "REROUTE");
  CHECK(r.transitSpeak({{"seq", 12}, {"on_route", true}, {"delay_minutes", 8}, {"slack_minutes", 5}}).verdict == "ALERT");
  CHECK(r.transitSpeak({{"seq", 10}, {"on_route", true}, {"delay_minutes", 1}}).verdict == "COMMENT");
  CHECK(r.transitSpeak({{"seq", 11}, {"on_route", true}, {"delay_minutes", 1}}).verdict == "IGNORE");
  CHECK(r.transitSpeak({{"seq", 11}, {"on_route", true}, {"stop_changed", true}}).verdict == "COMMENT");
}

TEST_CASE("rules: interrupt channel") {
  RulesReflex r;
  CHECK(r.interruptChannel({{"urgency", "URGENT"}, {"minutes_to_leave", 10}}).verdict == "CALL");
  CHECK(r.interruptChannel({{"urgency", "FIRM"}, {"minutes_to_leave", 10}, {"in_transit", true}}).verdict == "EARBUD");
  CHECK(r.interruptChannel({{"urgency", "FIRM"}, {"minutes_to_leave", 10}, {"time_of_day", "07:30"}}).verdict == "CALL");
  CHECK(r.interruptChannel({{"urgency", "FIRM"}, {"minutes_to_leave", 10}, {"time_of_day", "14:00"}}).verdict == "EARBUD");
  CHECK(r.interruptChannel({{"urgency", "CHILL"}, {"minutes_to_leave", 90}, {"time_of_day", "08:00"}}).verdict == "WAIT");
}

TEST_CASE("rules: readiness from the classified intent, or the text when unclear") {
  RulesReflex r;
  CHECK(r.readiness({{"intent", "AWAKE"}}).verdict == "READY");
  CHECK(r.readiness({{"intent", "SNOOZE_REQUEST"}}).verdict == "STALLING");
  CHECK(r.readiness({{"intent", "UNCLEAR"}, {"text", "I'm leaving now"}}).verdict == "READY");
  CHECK(r.readiness({{"text", "purple"}}).verdict == "UNCLEAR");
}

TEST_CASE("rules: snooze follows the consequences") {
  RulesReflex r;
  CHECK(r.snooze({{"requested_minutes", 5}, {"consequences", {{"worst", "OK"}}}}).verdict == "GRANT");
  CHECK(r.snooze({{"requested_minutes", 5}, {"consequences", {{"worst", "AT_RISK"}}}}).verdict == "COUNTER");
  CHECK(r.snooze({{"requested_minutes", 45}, {"consequences", {{"worst", "MISSED"}}}}).verdict == "REFUSE");
  CHECK(r.snooze({{"requested_minutes", 45}, {"consequences", {{"worst", "AT_RISK"}}}}).confidence >= 0.6);
}

TEST_CASE("rules: stop condition for the wake ladder") {
  RulesReflex r;
  CHECK(r.stopCondition({{"attempt", 1}, {"answered", false}, {"awake", false}}).verdict == "CONTINUE");
  CHECK(r.stopCondition({{"attempt", 3}, {"answered", false}, {"awake", false}}).verdict == "ESCALATE");
  CHECK(r.stopCondition({{"attempt", 2}, {"answered", true}, {"awake", true}}).verdict == "STOP");
  CHECK(r.stopCondition({{"attempt", 2}, {"answered", true}, {"awake", false}}).verdict == "CONTINUE");
  CHECK(r.stopCondition({{"attempt", 2}, {"minutes_since_alarm", 60}}).verdict == "BACK_OFF");
}
