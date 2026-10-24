// http/DashboardSocket. Shared.
#include <doctest/doctest.h>

#include "http/DashboardSocket.hpp"

using namespace enro;

TEST_CASE("broadcast wraps events in the envelope and replays history to new clients") {
  DashboardSocket ws(2);
  std::vector<std::string> a, b;
  ws.addClient(&a, [&](const std::string& t) { a.push_back(t); });
  ws.broadcast("decision", {{"verdict", "IGNORE"}});
  ws.broadcast("turn", {{"text", "hi"}});
  ws.broadcast("call_state", {{"state", "ringing"}});
  REQUIRE(a.size() == 3);
  auto msg = nlohmann::json::parse(a[0]);
  CHECK(msg["type"] == "decision");
  CHECK(msg.contains("ts"));
  CHECK(msg["data"]["verdict"] == "IGNORE");

  ws.addClient(&b, [&](const std::string& t) { b.push_back(t); });
  CHECK(b.size() == 2);  // history capped at 2
  ws.removeClient(&a);
  ws.broadcast("plan", {});
  CHECK(a.size() == 3);
  CHECK(b.size() == 3);
}
