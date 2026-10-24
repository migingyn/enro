// Enro backend. Ontime everytime.
// Boots in fake mode with an empty .env: sim calls, fake LLM, rules reflex, seed calendar and inbox.
#include <crow.h>

#include "app/App.hpp"
#include "http/Routes.hpp"
#include "store/Seed.hpp"
#include "util/Log.hpp"

int main() {
  auto ctx = enro::buildApp(enro::Env::load());

  if (ctx->env.demoMode()) enro::seedDemo(*ctx);
  ctx->scheduler->start();

  enro::CrowApp app;
  app.loglevel(crow::LogLevel::Warning);
  enro::registerRoutes(app, *ctx);

  int port = ctx->env.port();
  LOG_INFO("enro", "listening on http://localhost:" << port << "  (health: /health, dashboard: ws /ws/dashboard)");
  app.port(port).multithreaded().run();

  LOG_INFO("enro", "shutting down");
  return 0;
}
