// Judge dashboard feed: WS /ws/dashboard. Every message is an envelope
// {type, ts, data} with type in plan | decision | call_state | turn | trip_tick | sim_result
// (contracts/schemas/ws_*.schema.json). New clients get the recent history replayed.
// Crow-free on purpose so any module can broadcast; the Crow route lives in DashboardSocket.cpp.
#pragma once

#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace enro {

class DashboardSocket {
 public:
  using Sender = std::function<void(const std::string& text)>;

  explicit DashboardSocket(size_t history = 100) : history_(history) {}

  void addClient(void* conn, Sender send);  // replays recent() to the new client
  void removeClient(void* conn);
  // Wraps data in the envelope, sends it to every client, and keeps it in the history.
  void broadcast(const std::string& type, const nlohmann::json& data);

  std::vector<nlohmann::json> recent() const;
  size_t clients() const;

 private:
  size_t history_;
  mutable std::mutex mu_;
  std::map<void*, Sender> clients_;
  std::deque<nlohmann::json> recent_;
};

}  // namespace enro
