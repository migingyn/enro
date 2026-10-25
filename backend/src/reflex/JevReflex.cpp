#include "reflex/JevReflex.hpp"

#include <chrono>

#include <cpr/cpr.h>

#include "util/Log.hpp"

namespace enro {

using nlohmann::json;

namespace {

// ---------- every question Jev is asked, in one place ----------

const json kEmailQuestions = {
    {"changes_plan",
     {{"type", "noul"},
      {"instructions",
       "Does `email` tell the reader that one of their own classes, meetings, or appointments is canceled, moved to a "
       "different time, or moved to a different place?"},
      {"criteria",
       {{"true", "The email announces a cancellation, a new time, or a new room for something the reader attends."},
        {"false", "The email changes nothing the reader attends, for example a reminder, a confirmation, or a general "
                  "announcement."}}}}},
    {"about_schedule",
     {{"type", "noul"},
      {"instructions",
       "Is `email` about something on the reader's own schedule, such as a class, meeting, interview, appointment, or "
       "deadline?"},
      {"criteria",
       {{"true", "The email concerns an event or deadline the reader has."},
        {"false", "Advertising, a newsletter, a social message, or an automated notification with no event for the "
                  "reader."}}}}}};

const json kReadinessQuestions = {
    {"getting_up",
     {{"type", "noul"},
      {"instructions", "In `reply`, does the speaker say they are already up or getting up and getting ready right now?"},
      {"criteria",
       {{"true", "For example: \"I'm up\", \"getting dressed\", \"I'm in the shower\", or naming their first event."},
        {"false", "The speaker is still in bed, asking for time, or not answering."}}}}},
    {"asking_for_time",
     {{"type", "noul"},
      {"instructions", "In `reply`, is the speaker asking for more time or putting off getting up?"},
      {"criteria",
       {{"true", "For example: \"five more minutes\", \"later\", \"I'll get up soon\"."},
        {"false", "The speaker is not delaying, for example they say they are up or they answer a question."}}}}}};

// ---------- thresholds: the decisions, in code ----------

constexpr double kPlanChange = 0.6;   // changes_plan at or above -> PLAN_CHANGE
constexpr double kScheduleMaybe = 0.5;
constexpr double kReady = 0.6;
constexpr double kStalling = 0.6;

std::string str(const json& j, const char* key) { return j.is_object() && j.contains(key) && j[key].is_string() ? j[key].get<std::string>() : ""; }

}  // namespace

JevReflex::JevReflex(const Env& env, Transport transport)
    : api_key_(env.get("TYPESAFE_API_KEY")),
      model_(env.get("JEV_MODEL", "jev-latest")),
      base_url_(env.get("TYPESAFE_BASE_URL", "https://api.typesafe.ai")),
      timeout_ms_(env.getInt("JEV_TIMEOUT_MS", 800)),
      transport_(std::move(transport)) {
  if (api_key_.empty()) {
    LOG_WARN("jev", "REFLEX_PROVIDER=jev but TYPESAFE_API_KEY is empty, using rules");
    return;
  }
  if (!transport_) {
    transport_ = [this](const json& req) -> std::optional<json> {
      auto r = cpr::Post(cpr::Url{base_url_ + "/v1/systemone"},
                         cpr::Header{{"Authorization", "Bearer " + api_key_}, {"Content-Type", "application/json"}},
                         cpr::Body{req.dump()}, cpr::Timeout{timeout_ms_});
      if (r.error) {
        LOG_WARN("jev", "request failed after " << r.elapsed * 1000 << " ms: " << r.error.message);
        return std::nullopt;
      }
      if (r.status_code != 200) {
        LOG_WARN("jev", "HTTP " << r.status_code << ": " << r.text.substr(0, 200));
        return std::nullopt;
      }
      try {
        return json::parse(r.text);
      } catch (...) {
        LOG_WARN("jev", "response is not JSON");
        return std::nullopt;
      }
    };
  }
}

std::optional<JevReflex::NoulAnswers> JevReflex::askNouls(const json& state, const json& questions) {
  if (api_key_.empty() || !transport_) return std::nullopt;
  json req = {{"model", model_}, {"state", state}, {"questions", questions}};
  auto start = std::chrono::steady_clock::now();
  auto res = transport_(req);
  int ms = static_cast<int>(
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count());
  if (!res || !res->contains("answers")) return std::nullopt;
  NoulAnswers out{{}, ms};
  for (auto& [name, q] : questions.items()) {
    auto& a = (*res)["answers"];
    if (!a.contains(name) || !a[name].contains("noul") || !a[name]["noul"].is_number()) {
      LOG_WARN("jev", "answer missing for " << name);
      return std::nullopt;  // a partial answer is an error, not a "no"
    }
    out.p[name] = a[name]["noul"].get<double>();
  }
  LOG_DEBUG("jev", res->value("model", model_) << " answered in " << ms << " ms");
  return out;
}

ReflexVerdict JevReflex::emailRelevance(const json& email) {
  // Only what a person would read: sender, subject, snippet. Never a body, never internal ids.
  json state = {{"email", {{"from", str(email, "from")}, {"subject", str(email, "subject")}, {"snippet", str(email, "snippet")}}}};
  auto a = askNouls(state, kEmailQuestions);
  if (!a) return fallback_.emailRelevance(email);
  double change = a->p["changes_plan"], schedule = a->p["about_schedule"];
  if (change >= kPlanChange) return {"PLAN_CHANGE", change, "jev", a->latency_ms};
  if (schedule >= kScheduleMaybe) return {"MAYBE", schedule, "jev", a->latency_ms};
  return {"IGNORE", 1.0 - schedule, "jev", a->latency_ms};
}

ReflexVerdict JevReflex::readiness(const json& reply) {
  std::string text = str(reply, "text");
  if (text.empty()) return fallback_.readiness(reply);
  auto a = askNouls({{"reply", text}}, kReadinessQuestions);
  if (!a) return fallback_.readiness(reply);
  double up = a->p["getting_up"], delay = a->p["asking_for_time"];
  if (delay >= kStalling && delay > up) return {"STALLING", delay, "jev", a->latency_ms};
  if (up >= kReady) return {"READY", up, "jev", a->latency_ms};
  return {"UNCLEAR", 1.0 - std::max(up, delay), "jev", a->latency_ms};
}

// Arithmetic on numbers the code already has: rules are the right tool (TypeSafe skill, workflow step 1).
ReflexVerdict JevReflex::transitSpeak(const json& tick) { return fallback_.transitSpeak(tick); }
ReflexVerdict JevReflex::interruptChannel(const json& context) { return fallback_.interruptChannel(context); }
ReflexVerdict JevReflex::snooze(const json& request) { return fallback_.snooze(request); }
ReflexVerdict JevReflex::stopCondition(const json& state) { return fallback_.stopCondition(state); }

}  // namespace enro
