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
TEST_CASE("rules: 'canceled' in subject from a course address -> PLAN_CHANGE") {
  RulesReflex r;
  auto v = r.emailRelevance({{"message_id", "msg_1003"}, {"from", "cse110-tas@ucsd.edu"}, {"subject", "Section canceled today"}});
  CHECK(v.verdict == "PLAN_CHANGE");
  CHECK(v.confidence >= 0.9);
  CHECK(v.source == "rules");
  CHECK(r.emailRelevance({{"from", "prof@ucsd.edu"}, {"subject", "Lecture moved to Center 115"}}).verdict == "PLAN_CHANGE");
  CHECK(r.emailRelevance({{"from", "noreply@ucsd.edu"}, {"subject", "Midterm rescheduled"}}).verdict == "PLAN_CHANGE");
}

TEST_CASE("rules: promos are ignored, schedule mentions are MAYBE") {
  RulesReflex r;
  CHECK(r.emailRelevance({{"from", "deals@shop.com"}, {"subject", "Order canceled? 20% off today"}}).verdict == "IGNORE");
  CHECK(r.emailRelevance({{"from", "friend@gmail.com"}, {"subject", "Lunch?"}, {"snippet", "hungry"}}).verdict == "IGNORE");
  CHECK(r.emailRelevance({{"from", "tutor@ucsd.edu"}, {"subject", "Office hours reminder"}}).verdict == "MAYBE");
}

TEST_CASE("rules: off-route tick -> REROUTE") {
  RulesReflex r;
  CHECK(r.transitSpeak({{"seq", 17}, {"delay_minutes", 7}, {"on_route", false}}).verdict == "REROUTE");
}

TEST_CASE("rules: transit delay past slack alerts once, quiet stops comment every few") {
  RulesReflex r;
  CHECK(r.transitSpeak({{"seq", 3}, {"delay_minutes", 2}, {"on_route", true}}).verdict == "IGNORE");
  CHECK(r.transitSpeak({{"seq", 5}, {"delay_minutes", 2}, {"on_route", true}}).verdict == "COMMENT");
  CHECK(r.transitSpeak({{"seq", 6}, {"delay_minutes", 7}, {"on_route", true}}).verdict == "ALERT");
  CHECK(r.transitSpeak({{"seq", 7}, {"delay_minutes", 7}, {"prev_delay_minutes", 4}}).verdict == "ALERT");
  CHECK(r.transitSpeak({{"seq", 8}, {"delay_minutes", 7}, {"prev_delay_minutes", 7}}).verdict == "IGNORE");
  CHECK(r.transitSpeak({{"seq", 9}, {"delay_minutes", 3}, {"slack_minutes", 2}}).verdict == "ALERT");
}

TEST_CASE("rules: interrupt channel") {
  RulesReflex r;
  CHECK(r.interruptChannel({{"urgency", "URGENT"}, {"minutes_to_leave", 5}, {"in_transit", true}}).verdict == "EARBUD");
  CHECK(r.interruptChannel({{"urgency", "URGENT"}, {"minutes_to_leave", 40}, {"time_of_day", "14:00"}}).verdict == "CALL");
  CHECK(r.interruptChannel({{"urgency", "CHILL"}, {"minutes_to_leave", 40}, {"time_of_day", "07:30"}}).verdict == "CALL");
  CHECK(r.interruptChannel({{"urgency", "FIRM"}, {"minutes_to_leave", 10}, {"time_of_day", "14:00"}}).verdict == "CALL");
  CHECK(r.interruptChannel({{"urgency", "FIRM"}, {"minutes_to_leave", 40}, {"time_of_day", "14:00"}}).verdict == "WAIT");
  CHECK(r.interruptChannel({{"urgency", "FIRM"}, {"minutes_to_leave", 300}, {"time_of_day", "07:00"}}).verdict == "WAIT");
}

TEST_CASE("rules: readiness from the classified intent, or the text when no intent is given") {
  RulesReflex r;
  CHECK(r.readiness({{"text", "getting dressed"}, {"intent", "READY"}}).verdict == "READY");
  CHECK(r.readiness({{"intent", "LEFT"}}).verdict == "READY");
  CHECK(r.readiness({{"intent", "STALLING"}}).verdict == "STALLING");
  CHECK(r.readiness({{"intent", "SNOOZE_REQUEST"}}).verdict == "STALLING");
  CHECK(r.readiness({{"intent", "UNCLEAR"}}).verdict == "UNCLEAR");
  CHECK(r.readiness({{"text", "ugh go away"}}).verdict == "STALLING");
}

TEST_CASE("rules: snooze follows the consequences at the inflated delay") {
  RulesReflex r;
  auto req = [](const char* worst) {
    return nlohmann::json{{"requested_minutes", 10}, {"snooze_inflation", 1.4}, {"consequences", {{"worst", worst}}}};
  };
  CHECK(r.snooze(req("OK")).verdict == "GRANT");
  CHECK(r.snooze(req("AT_RISK")).verdict == "COUNTER");
  CHECK(r.snooze(req("MISSED")).verdict == "REFUSE");
  CHECK(r.snooze(req("OK")).confidence >= 0.6);  // confident enough that Conversation keeps it
  CHECK(r.snooze({{"requested_minutes", 10}}).confidence < 0.6);
}

TEST_CASE("rules: stop condition") {
  RulesReflex r;
  CHECK(r.stopCondition({{"attempt", 1}, {"answered", false}, {"awake", false}}).verdict == "CONTINUE");
  CHECK(r.stopCondition({{"attempt", 2}, {"answered", true}, {"awake", false}}).verdict == "BACK_OFF");
  CHECK(r.stopCondition({{"attempt", 3}, {"answered", false}, {"awake", false}}).verdict == "ESCALATE");
  CHECK(r.stopCondition({{"attempt", 3}, {"answered", true}, {"awake", true}}).verdict == "STOP");
}
