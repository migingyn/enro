// HTTP + websocket routes (PRD 11). registerRoutes wires every file below; each task edits
// only its own register*() file.
#pragma once

#include <crow.h>

#include <optional>

#include <nlohmann/json.hpp>

namespace enro {

struct AppContext;
using CrowApp = crow::SimpleApp;

void registerRoutes(CrowApp& app, AppContext& ctx);

void registerApiRoutes(CrowApp& app, AppContext& ctx);         // ApiRoutes.cpp (shared)
void registerOnboardingRoutes(CrowApp& app, AppContext& ctx);  // OnboardingRoutes.cpp (T01)
void registerOAuthRoutes(CrowApp& app, AppContext& ctx);       // OAuthRoutes.cpp (T03)
void registerTwilioRoutes(CrowApp& app, AppContext& ctx);      // TwilioRoutes.cpp (T02)
void registerDashboardSocket(CrowApp& app, AppContext& ctx);   // DashboardSocket.cpp (shared)
void registerSimCallSocket(CrowApp& app, AppContext& ctx);     // SimCallSocket.cpp (T02)
void registerMotionRoutes(CrowApp& app, AppContext& ctx);      // MotionRoutes.cpp (S7)
// void registerRetellRoutes(CrowApp& app, AppContext& ctx);   // RetellRoutes.cpp (RETELL ALTERNATE)

// ---- response helpers ----
crow::response jsonResponse(const nlohmann::json& body, int code = 200);
crow::response errorResponse(int code, const std::string& error, const std::string& message);
// contracts/examples/<name>.json as the response body. Used by every // STUB route.
crow::response fixtureResponse(AppContext& ctx, const std::string& name);
crow::response xmlResponse(const std::string& xml);
crow::response redirectResponse(const std::string& url);
// Parses a JSON request body, or nullopt (caller returns 400).
std::optional<nlohmann::json> jsonBody(const crow::request& req);

}  // namespace enro
