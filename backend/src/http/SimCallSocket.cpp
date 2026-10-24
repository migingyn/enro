// WS /ws/sim-call/:call_id: the browser call simulator (CallSimulator page). Connects the page
// to SimProvider. Messages: contracts/schemas/simcall_*.schema.json. Owner: T02 (voice).
#include "app/App.hpp"
#include "http/Routes.hpp"
#include "util/Log.hpp"

namespace enro {

namespace {

std::string callIdFromUrl(const std::string& url) {
  auto slash = url.find_last_of('/');
  std::string id = slash == std::string::npos ? url : url.substr(slash + 1);
  if (auto q = id.find('?'); q != std::string::npos) id = id.substr(0, q);
  return id;
}

}  // namespace

void registerSimCallSocket(CrowApp& app, AppContext& ctx) {
  CROW_WEBSOCKET_ROUTE(app, "/ws/sim-call/<string>")
      .onaccept([&ctx](const crow::request& req, void** userdata) {
        if (!ctx.sim) return false;  // only available with VOICE_PROVIDER=sim
        *userdata = new std::string(callIdFromUrl(req.url));
        return true;
      })
      .onopen([&ctx](crow::websocket::connection& conn) {
        auto* call_id = static_cast<std::string*>(conn.userdata());
        LOG_INFO("sim", "browser joined " << *call_id);
        ctx.sim->attachBrowser(*call_id, &conn, [&conn](const std::string& text) { conn.send_text(text); });
      })
      .onmessage([&ctx](crow::websocket::connection& conn, const std::string& data, bool is_binary) {
        if (is_binary) return;
        ctx.sim->onBrowserMessage(*static_cast<std::string*>(conn.userdata()), data);
      })
      .onclose([&ctx](crow::websocket::connection& conn, const std::string&, uint16_t) {
        ctx.sim->detachBrowser(&conn);
        delete static_cast<std::string*>(conn.userdata());
        conn.userdata(nullptr);
      });
}

}  // namespace enro
