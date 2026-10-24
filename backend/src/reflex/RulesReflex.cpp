#include "reflex/RulesReflex.hpp"

#include <algorithm>
#include <cctype>
#include <regex>
#include <string>

#include "ai/IntentClassifier.hpp"

namespace enro {

// Deterministic rules for every reflex decision (PRD 7.11). Default provider, and what JevReflex
// falls back to on timeout or a missing key. Confidence is >= 0.6 so callers trust these verdicts.

namespace {

std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
  return s;
}

bool has(const std::string& s, std::initializer_list<const char*> needles) {
  return std::any_of(needles.begin(), needles.end(), [&](const char* n) { return s.find(n) != std::string::npos; });
}

std::string str(const nlohmann::json& j, const char* key) {
  return j.is_object() && j.contains(key) && j[key].is_string() ? j[key].get<std::string>() : "";
}

int num(const nlohmann::json& j, const char* key, int fallback) {
  return j.is_object() && j.contains(key) && j[key].is_number() ? j[key].get<int>() : fallback;
}

bool flag(const nlohmann::json& j, const char* key) {
  return j.is_object() && j.contains(key) && j[key].is_boolean() && j[key].get<bool>();
}

}  // namespace

ReflexVerdict RulesReflex::emailRelevance(const nlohmann::json& email) {
  std::string from = lower(str(email, "from"));
  std::string text = lower(str(email, "subject") + " " + str(email, "snippet"));

  bool change = has(text, {"cancel", "rescheduled", "reschedule", "postponed", "moved to", "room change", "new room",
                           "new location", "location change", "has moved", "pushed to", "pushed back", "delayed",
                           "no class", "no section", "no lecture", "will not meet", "won't meet", "gate change"}) ||
                std::regex_search(text, std::regex(R"(\bnow (at|in|starts)\b)"));
  bool promo = has(text, {"unsubscribe", "newsletter", "% off", "sale", "deal", "discount", "promo", "coupon",
                          "limited time", "digest", "webinar", "sponsored", "free trial", "rewards"}) ||
               has(from, {"newsletter", "marketing", "promo", "deals", "offers", "news@", "noreply@medium", "digest"});
  bool trusted = has(from, {".edu", "calendar", "airline", "recruit", "talent", "hr@", "careers"});

  if (change && (trusted || !promo)) return {"PLAN_CHANGE", trusted ? 0.9 : 0.75, "rules", 0};
  if (promo) return {"IGNORE", 0.9, "rules", 0};
  if (trusted || has(text, {"reminder", "tomorrow", "today", "this week", "schedule", "meeting", "interview",
                            "office hours", "deadline", "due ", "rsvp", "confirm"}))
    return {"MAYBE", 0.7, "rules", 0};
  return {"IGNORE", 0.7, "rules", 0};
}

ReflexVerdict RulesReflex::transitSpeak(const nlohmann::json& tick) {
  if (tick.is_object() && tick.contains("on_route") && tick["on_route"].is_boolean() && !tick["on_route"].get<bool>())
    return {"REROUTE", 0.95, "rules", 0};
  int delay = num(tick, "delay_minutes", 0);
  int slack = num(tick, "slack_minutes", 5);  // minutes of buffer left before the user is late
  if (delay > slack) return {"ALERT", 0.85, "rules", 0};
  // Don't narrate every tick: speak at new stops or every fifth tick.
  if (flag(tick, "stop_changed") || num(tick, "seq", 1) % 5 == 0) return {"COMMENT", 0.7, "rules", 0};
  return {"IGNORE", 0.8, "rules", 0};
}

ReflexVerdict RulesReflex::interruptChannel(const nlohmann::json& ctx) {
  std::string urgency = str(ctx, "urgency");
  if (flag(ctx, "in_transit")) return {urgency == "URGENT" ? "CALL" : "EARBUD", 0.8, "rules", 0};
  if (urgency == "URGENT") return {"CALL", 0.9, "rules", 0};
  std::string tod = str(ctx, "time_of_day");  // "HH:MM" or morning | day | evening
  bool morning = tod == "morning" || (tod.size() >= 2 && std::isdigit(static_cast<unsigned char>(tod[0])) &&
                                      std::stoi(tod.substr(0, 2)) < 10);
  int minutes = num(ctx, "minutes_to_leave", 0);
  if (minutes > 30) return {"WAIT", 0.8, "rules", 0};
  if (morning) return {"CALL", 0.8, "rules", 0};
  return {"EARBUD", 0.75, "rules", 0};
}

ReflexVerdict RulesReflex::readiness(const nlohmann::json& reply) {
  std::string intent = str(reply, "intent");
  if (intent.empty() || intent == "UNCLEAR") intent = IntentClassifier::keywords(str(reply, "text")).label;
  if (intent == "READY" || intent == "AWAKE" || intent == "LEFT") return {"READY", 0.8, "rules", 0};
  if (intent == "STALLING" || intent == "SNOOZE_REQUEST") return {"STALLING", 0.8, "rules", 0};
  return {"UNCLEAR", 0.6, "rules", 0};
}

ReflexVerdict RulesReflex::snooze(const nlohmann::json& request) {
  // Same policy as the Consequence Engine: OK after inflation -> GRANT, AT_RISK -> COUNTER, MISSED -> REFUSE.
  std::string worst = request.is_object() && request.contains("consequences") && request["consequences"].is_object()
                          ? str(request["consequences"], "worst")
                          : "";
  if (worst == "OK") return {"GRANT", 0.9, "rules", 0};
  if (worst == "MISSED") return {"REFUSE", 0.9, "rules", 0};
  return {"COUNTER", worst == "AT_RISK" ? 0.9 : 0.6, "rules", 0};
}

ReflexVerdict RulesReflex::stopCondition(const nlohmann::json& state) {
  if (flag(state, "awake")) return {"STOP", 0.95, "rules", 0};
  if (flag(state, "answered")) return {"CONTINUE", 0.8, "rules", 0};  // on the line, not verified yet
  int attempt = num(state, "attempt", 1);
  if (attempt >= 3) return {"ESCALATE", 0.9, "rules", 0};
  // Long past the alarm with nobody picking up: space the calls out instead of ringing every 3 minutes.
  if (num(state, "minutes_since_alarm", 0) > 45) return {"BACK_OFF", 0.7, "rules", 0};
  return {"CONTINUE", 0.85, "rules", 0};
}

}  // namespace enro
