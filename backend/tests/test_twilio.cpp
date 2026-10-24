// T02: voice providers (SimProvider, TwilioProvider, TwiML).
#include <doctest/doctest.h>

#include "voice/SimProvider.hpp"
#include "voice/TwiML.hpp"
#include "voice/TwilioProvider.hpp"

using namespace enro;

TEST_CASE("SimProvider queues messages until the browser connects, then relays events") {
  SimProvider sim;
  std::vector<std::string> events;
  sim.onCallEvent([&](const std::string&, const std::string& ev, const std::string&) { events.push_back(ev); });
  CHECK(sim.placeCall({"c1", "u", "+1", "wake", 1, "best_friend", Urgency::CHILL}) == "SIM-c1");

  std::vector<std::string> got;
  int conn = 0;
  sim.attachBrowser("c1", &conn, [&](const std::string& t) { got.push_back(t); });
  REQUIRE(got.size() == 1);
  CHECK(nlohmann::json::parse(got[0])["type"] == "ring");

  sim.onBrowserMessage("c1", R"({"type":"answer","ts":0,"data":{"call_id":"c1"}})");
  sim.onBrowserMessage("c1", R"({"type":"speech","ts":0,"data":{"call_id":"c1","text":"five more"}})");
  CHECK(events == std::vector<std::string>{"answered", "speech"});
}

TEST_CASE("TwiML escapes text") {
  CHECK(twiml::say("a < b & c") == "<Say>a &lt; b &amp; c</Say>");
  CHECK(twiml::response("").find("<Response></Response>") != std::string::npos);
}

TEST_CASE("TwilioProvider constructs without credentials") {
  TwilioProvider t(Env::fromMap({}));
  CHECK(t.name() == "twilio");
}

TEST_CASE("TODO(T02): validSignature accepts Twilio's documented example and rejects a tampered one" * doctest::skip()) {}
TEST_CASE("TODO(T02): /twilio/voice renders the opener with Play + Gather" * doctest::skip()) {}
TEST_CASE("TODO(T02): /twilio/status no-answer emits no_answer" * doctest::skip()) {}
TEST_CASE("TODO(T02): inbound STOP revokes the contact" * doctest::skip()) {}
