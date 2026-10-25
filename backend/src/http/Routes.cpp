#include "http/Routes.hpp"

#include "app/App.hpp"
#include "util/Log.hpp"

namespace enro {

void registerRoutes(CrowApp& app, AppContext& ctx) {
  registerApiRoutes(app, ctx);
  registerOnboardingRoutes(app, ctx);
  registerOAuthRoutes(app, ctx);
  registerTwilioRoutes(app, ctx);
  registerDashboardSocket(app, ctx);
  registerSimCallSocket(app, ctx);
  registerMotionRoutes(app, ctx);
  // registerRetellRoutes(app, ctx);  // RETELL ALTERNATE
}

crow::response jsonResponse(const nlohmann::json& body, int code) {
  crow::response res(code, body.dump());
  res.set_header("Content-Type", "application/json");
  return res;
}

crow::response errorResponse(int code, const std::string& error, const std::string& message) {
  return jsonResponse({{"error", error}, {"message", message}}, code);
}

crow::response fixtureResponse(AppContext& ctx, const std::string& name) {
  try {
    return jsonResponse(ctx.fixtures.json(name));
  } catch (const std::exception& e) {
    LOG_ERROR("http", e.what());
    return errorResponse(500, "fixture_missing", e.what());
  }
}

crow::response xmlResponse(const std::string& xml) {
  crow::response res(200, xml);
  res.set_header("Content-Type", "text/xml");
  return res;
}

crow::response redirectResponse(const std::string& url) {
  crow::response res(302);
  res.set_header("Location", url);
  return res;
}

std::optional<nlohmann::json> jsonBody(const crow::request& req) {
  try {
    auto j = nlohmann::json::parse(req.body.empty() ? "{}" : req.body);
    if (!j.is_object()) return std::nullopt;
    return j;
  } catch (...) {
    return std::nullopt;
  }
}

}  // namespace enro
