#include "reflex/RulesReflex.hpp"

#include <algorithm>
#include <cctype>

#include "ai/IntentClassifier.hpp"

namespace enro {

// Keyword and threshold rules for every decision. They are the default provider and the fallback
// when Jev times out, so they stay cheap and deterministic.

namespace {

std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
  return s;
}

bool has(const std::string& s, std::initializer_list<const char*> needles) {
  return std::any_of(needles.begin(), needles.end(), [&](const char* n) { return s.find(n) != std::string::npos; });
}

std::string str(const nlohmann::json& j, const char* key) {
  auto it = j.find(key);
  return it != j.end() && it->is_string() ? it->get<std::string>() : "";
}

int num(const nlohmann::json& j, const char* key, int fallback) {
  auto it = j.find(key);
  return it != j.end() && it->is_number() ? it->get<int>() : fallback;
}

bool flag(const nlohmann::json& j, const char* key) {
  auto it = j.find(key);
  return it != j.end() && it->is_boolean() && it->get<bool>();
}

// "HH:MM" -> hour, or -1 when missing or malformed.
int hourOf(const std::string& hhmm) {
  if (hhmm.size() < 2 || !std::isdigit(static_cast<unsigned char>(hhmm[0])) ||
      !std::isdigit(static_cast<unsigned char>(hhmm[1])))
    return -1;
  return (hhmm[0] - '0') * 10 + (hhmm[1] - '0');
}

constexpr int kDefaultSlackMinutes = 5;  // transit delay the plan absorbs before we alert
constexpr int kCommentEveryStops = 5;    // light commentary cadence on a quiet ride
constexpr int kCallWithinMinutes = 15;   // a leave time this close is worth a call
constexpr int kNothingDueMinutes = 60;   // further out than this, stay quiet
constexpr int kMorningEndHour = 10;      // before this, the user is likely still in bed

}  // namespace

ReflexVerdict RulesReflex::emailRelevance(const nlohmann::json& email) {
  std::string from = lower(str(email, "from"));
  std::string text = lower(str(email, "subject") + " " + str(email, "snippet"));

  bool school = has(from, {".edu"});
  // School systems send from noreply too, so only non-school senders count as promotional.
  bool promo = !school && (has(from, {"noreply", "no-reply", "newsletter", "marketing", "deals"}) ||
                           has(text, {"unsubscribe", "% off", " sale", "promo", "coupon"}));
  bool change = has(text, {"cancel", "moved", "rescheduled", "postponed", "room change", "new room", "new location",
                           "location change", "pushed back", "no class", "no section", "no lecture", "delayed"});
  bool schedule = has(text, {"class", "lecture", "section", "discussion", "exam", "midterm", "quiz", "interview",
                             "meeting", "office hours", "today", "tomorrow"});

  if (promo) return {"IGNORE", 0.9};
  if (change) return {"PLAN_CHANGE", school ? 0.9 : 0.75};
  if (schedule) return {"MAYBE", 0.6};
  return {"IGNORE", 0.7};
}

ReflexVerdict RulesReflex::transitSpeak(const nlohmann::json& tick) {
  if (tick.contains("on_route") && tick["on_route"].is_boolean() && !tick["on_route"].get<bool>())
    return {"REROUTE", 0.95};

  int delay = num(tick, "delay_minutes", 0);
  int slack = num(tick, "slack_minutes", kDefaultSlackMinutes);
  // With the previous delay, alert only on the tick that crosses the slack (or grows a lot past it).
  int prev = num(tick, "prev_delay_minutes", -1);
  if (delay > slack && (prev < 0 || prev <= slack || delay - prev >= 3)) return {"ALERT", 0.9};

  int seq = num(tick, "seq", 0);
  if (seq > 0 && seq % kCommentEveryStops == 0) return {"COMMENT", 0.7};
  return {"IGNORE", 0.8};
}

ReflexVerdict RulesReflex::interruptChannel(const nlohmann::json& context) {
  if (flag(context, "in_transit")) return {"EARBUD", 0.9};

  bool urgent = str(context, "urgency") == "URGENT";
  int minutes = num(context, "minutes_to_leave", -1);
  bool nothing_due = minutes < 0 || minutes > kNothingDueMinutes;
  if (nothing_due && !urgent) return {"WAIT", 0.85};

  int hour = hourOf(str(context, "time_of_day"));
  bool morning = hour >= 0 && hour < kMorningEndHour;
  if (urgent || morning || minutes <= kCallWithinMinutes) return {"CALL", 0.85};
  return {"WAIT", 0.7};
}

ReflexVerdict RulesReflex::readiness(const nlohmann::json& reply) {
  std::string intent = str(reply, "intent");
  if (intent.empty() && !str(reply, "text").empty()) intent = IntentClassifier::keywords(str(reply, "text")).label;

  if (intent == "READY" || intent == "LEFT") return {"READY", 0.85};
  if (intent == "STALLING" || intent == "SNOOZE_REQUEST") return {"STALLING", 0.8};
  return {"UNCLEAR", 0.5};
}

ReflexVerdict RulesReflex::snooze(const nlohmann::json& request) {
  // The consequences were already evaluated at the inflated (effective) snooze, so their worst case decides.
  std::string worst = request.contains("consequences") && request["consequences"].is_object()
                          ? str(request["consequences"], "worst")
                          : "";
  if (worst == "OK") return {"GRANT", 0.9};
  if (worst == "AT_RISK") return {"COUNTER", 0.9};
  if (worst == "MISSED") return {"REFUSE", 0.9};
  return {"COUNTER", 0.5};  // no consequences: low confidence so the caller's policy decides
}

ReflexVerdict RulesReflex::stopCondition(const nlohmann::json& state) {
  if (flag(state, "awake")) return {"STOP", 0.95};

  bool answered = flag(state, "answered");
  if (!answered && num(state, "attempt", 1) >= 3) return {"ESCALATE", 0.9};
  // Picked up but not verified awake yet: give them a moment before ringing again.
  if (answered) return {"BACK_OFF", 0.7};
  return {"CONTINUE", 0.8};
}

}  // namespace enro
