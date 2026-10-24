#include "http/DashboardSocket.hpp"

#include "app/App.hpp"
#include "http/Routes.hpp"
#include "util/Log.hpp"
#include "util/Time.hpp"

namespace enro {

void DashboardSocket::addClient(void* conn, Sender send) {
  std::vector<nlohmann::json> replay;
  {
    std::lock_guard<std::mutex> lock(mu_);
    clients_[conn] = send;
    replay.assign(recent_.begin(), recent_.end());
  }
  for (auto& msg : replay) send(msg.dump());
  LOG_INFO("dashboard", "client connected (" << clients() << " open), replayed " << replay.size() << " events");
}

void DashboardSocket::removeClient(void* conn) {
  std::lock_guard<std::mutex> lock(mu_);
  clients_.erase(conn);
}

void DashboardSocket::broadcast(const std::string& type, const nlohmann::json& data) {
  nlohmann::json msg = {{"type", type}, {"ts", time::nowTs()}, {"data", data}};
  std::string text = msg.dump();
  std::vector<Sender> targets;
  {
    std::lock_guard<std::mutex> lock(mu_);
    recent_.push_back(std::move(msg));
    while (recent_.size() > history_) recent_.pop_front();
    for (auto& [conn, send] : clients_) targets.push_back(send);
  }
  for (auto& send : targets) send(text);
  LOG_DEBUG("dashboard", "broadcast " << type << " to " << targets.size() << " clients");
}

std::vector<nlohmann::json> DashboardSocket::recent() const {
  std::lock_guard<std::mutex> lock(mu_);
  return {recent_.begin(), recent_.end()};
}

size_t DashboardSocket::clients() const {
  std::lock_guard<std::mutex> lock(mu_);
  return clients_.size();
}

void registerDashboardSocket(CrowApp& app, AppContext& ctx) {
  CROW_WEBSOCKET_ROUTE(app, "/ws/dashboard")
      .onopen([&ctx](crow::websocket::connection& conn) {
        ctx.dashboard->addClient(&conn, [&conn](const std::string& text) { conn.send_text(text); });
      })
      .onclose([&ctx](crow::websocket::connection& conn, const std::string&, uint16_t) {
        ctx.dashboard->removeClient(&conn);
      })
      .onmessage([](crow::websocket::connection&, const std::string&, bool) {
        // The dashboard only listens. Demo controls use POST /api/demo/*.
      });
}

}  // namespace enro
