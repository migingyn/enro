// Google sign-in (PRD 5 step 4). Owner: T03.
// Register this redirect URI in Google Cloud: <PUBLIC_BASE_URL or http://localhost:PORT>/oauth/google/callback
// In fake mode (no GOOGLE_CLIENT_ID), start redirects straight to the callback, which lands on /done.
#include "app/App.hpp"
#include "http/Routes.hpp"
#include "util/Log.hpp"

namespace enro {

void registerOAuthRoutes(CrowApp& app, AppContext& ctx) {
  CROW_ROUTE(app, "/oauth/google/start")([&ctx](const crow::request& req) {
    const char* user = req.url_params.get("user_id");
    return redirectResponse(ctx.google_auth->authUrl(user ? user : "u_demo"));
  });

  CROW_ROUTE(app, "/oauth/google/callback")([&ctx](const crow::request& req) {
    const char* code = req.url_params.get("code");
    const char* state = req.url_params.get("state");
    if (!code) return errorResponse(400, "missing_code", "Google did not return an authorization code");
    bool ok = ctx.google_auth->handleCallback(code, state ? state : "u_demo");
    std::string frontend = ctx.env.get("FRONTEND_URL", "http://localhost:5173");
    return redirectResponse(frontend + (ok ? "/done" : "/onboarding?error=google"));
  });
}

}  // namespace enro
