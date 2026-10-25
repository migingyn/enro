// Voice onboarding backend (PRD 5, 11). Owner: T01 (Realtime key, profile extraction).
// The page itself is T17 (frontend).
#include "http/OnboardingRoutes.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "app/App.hpp"
#include "http/Routes.hpp"
#include "models/Json.hpp"
#include "util/Log.hpp"
#include "util/Time.hpp"

namespace enro {

namespace {

// The persona whose harshness is closest to what the user asked for; ties go to the gentler one.
// Returns that archetype from personas.json (empty object when there are none).
nlohmann::json pickArchetype(const nlohmann::json& personas, double harshness) {
  nlohmann::json best = nlohmann::json::object();
  double best_d = 2, best_h = 2;
  for (auto& a : personas.value("archetypes", nlohmann::json::array())) {
    if (!a.is_object() || !a.contains("id")) continue;
    double h = a.value("harshness", 0.5);
    double d = std::round(std::fabs(h - harshness) * 1000) / 1000;
    if (d < best_d || (d == best_d && h < best_h)) best = a, best_d = d, best_h = h;
  }
  return best;
}

std::string slug(const std::string& s) {
  std::string out;
  for (unsigned char c : s) {
    if (std::isalnum(c)) out += static_cast<char>(std::tolower(c));
    else if (!out.empty() && out.back() != '_') out += '_';
  }
  while (!out.empty() && out.back() == '_') out.pop_back();
  return out.empty() ? "contact" : out;
}

}  // namespace

nlohmann::json completeOnboarding(AppContext& ctx, const nlohmann::json& body, long long now) {
  const nlohmann::json profile = body.value("profile", nlohmann::json::object());
  std::string id = body.value("user_id", "");
  if (id.empty()) id = ctx.env.getBool("DEMO_MODE", true) ? "u_demo" : "u_" + std::to_string(now);

  User u = ctx.repos->user(id).value_or(User{});
  u.id = id;
  auto str = [&](const char* key, std::string& field) {
    if (profile.contains(key) && profile[key].is_string() && !profile[key].get<std::string>().empty())
      field = profile[key].get<std::string>();
  };
  str("name", u.name);
  str("phone", u.phone);
  str("timezone", u.timezone);
  str("home_area", u.home_area);
  str("alarm_time", u.alarm_time);
  if (u.timezone.empty()) u.timezone = "America/Los_Angeles";
  if (profile.contains("preferred_mode") && profile["preferred_mode"].is_object())
    u.preferred_mode = profile["preferred_mode"].get<std::map<std::string, std::string>>();
  if (profile.contains("prep_minutes") && profile["prep_minutes"].is_object())
    u.prep_minutes = profile["prep_minutes"].get<std::map<std::string, int>>();

  double harshness = std::clamp(profile.value("harshness_pref", 0.5), 0.0, 1.0);
  nlohmann::json a = pickArchetype(ctx.personas, harshness);
  u.archetype_id = a.value("id", "best_friend");
  u.warmth = a.value("warmth", 0.5);
  u.humor = a.value("humor", 0.5);
  u.verbosity = a.value("verbosity", 0.5);
  u.harshness = harshness;
  ctx.repos->upsertUser(u);

  nlohmann::json saved = nlohmann::json::array();
  for (auto& c : body.value("contacts", nlohmann::json::array())) {
    std::string phone = c.value("phone", "");
    if (phone.empty()) continue;
    EmergencyContact ec;
    ec.user_id = id;
    ec.spoken_name = c.value("spoken_name", "");
    ec.relationship = c.value("relationship", "");
    ec.phone = phone;
    ec.id = "c_" + slug(ec.spoken_name.empty() ? phone : ec.spoken_name) + "_" + id;
    // Re-saying a contact keeps its consent; a new one always starts PENDING.
    auto existing = ctx.repos->contact(ec.id);
    ec.consent = existing && existing->phone == phone ? existing->consent : ConsentStatus::PENDING;
    ec.added_ts = existing ? existing->added_ts : now;
    ctx.repos->upsertContact(ec);
    saved.push_back(ec);
  }
  LOG_INFO("onboarding", "saved " << id << " (" << u.archetype_id << ") with " << saved.size() << " contact(s)");
  return {{"user", u}, {"contacts", saved}, {"oauth_url", "/oauth/google/start?user_id=" + id}};
}

void registerOnboardingRoutes(CrowApp& app, AppContext& ctx) {
  // POST /api/onboarding/session -> ephemeral Realtime key + interview instructions
  CROW_ROUTE(app, "/api/onboarding/session").methods(crow::HTTPMethod::POST)([&ctx] {
    nlohmann::json res = ctx.fixtures.json("onboarding_session");
    // Questions and instructions come from backend/config/onboarding.json when it exists.
    if (ctx.onboarding.contains("questions")) res["questions"] = ctx.onboarding["questions"];
    if (ctx.onboarding.contains("instructions")) res["instructions"] = ctx.onboarding["instructions"];
    // STUB
    // TODO(T01): with LLM speech on OpenAI, mint a real key: ctx.speech->realtimeSession(instructions) and set
    // provider "openai", client_secret, expires_at, model, voice. Keep provider "fake" otherwise.
    return jsonResponse(res);
  });

  // POST /api/onboarding/complete -> save the structured profile and contacts
  CROW_ROUTE(app, "/api/onboarding/complete").methods(crow::HTTPMethod::POST)([&ctx](const crow::request& req) {
    auto body = jsonBody(req);
    if (!body || !body->contains("profile") || !(*body)["profile"].is_object())
      return errorResponse(400, "bad_request", "expected JSON body (onboarding_complete_request)");
    return jsonResponse(completeOnboarding(ctx, *body, time::nowTs()));
  });
}

}  // namespace enro
