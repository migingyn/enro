// Voice onboarding backend (PRD 5, 11). Owner: T01 (Realtime key, profile extraction).
// The page itself is T17 (frontend).
#include "app/App.hpp"
#include "http/Routes.hpp"
#include "models/Json.hpp"

namespace enro {

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
    if (!jsonBody(req)) return errorResponse(400, "bad_request", "expected JSON body (onboarding_complete_request)");
    // STUB
    // TODO(T01): build a User from body.profile (id, archetype picked from answers, warmth/harshness from
    // harshness_pref), save with ctx.repos->upsertUser; save each contact as PENDING with ctx.repos->upsertContact;
    // return {user, contacts, oauth_url: "/oauth/google/start?user_id=<id>"}.
    return fixtureResponse(ctx, "onboarding_complete");
  });
}

}  // namespace enro
