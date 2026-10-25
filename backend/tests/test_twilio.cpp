// T02: voice providers (SimProvider, TwilioProvider, TwiML).
#include <doctest/doctest.h>

#include <cctype>
#include <cstdio>

#include "TestApp.hpp"
#include "http/Routes.hpp"
#include "store/Seed.hpp"
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

namespace {

// Fake but well-formed settings. PUBLIC_BASE_URL is http:// on purpose: the provider then refuses to
// call the REST API, so no test ever reaches api.twilio.com.
const std::string kBase = "http://twilio-test.local";
std::map<std::string, std::string> twilioEnv() {
  return {{"VOICE_PROVIDER", "twilio"},
          {"TWILIO_ACCOUNT_SID", "AC" + std::string(32, '0')},
          {"TWILIO_AUTH_TOKEN", "fake_auth_token_0123456789"},
          {"TWILIO_FROM_NUMBER", "+15005550006"},
          {"PUBLIC_BASE_URL", kBase}};
}

std::string formEncode(const std::map<std::string, std::string>& params) {
  std::string out;
  for (auto& [k, v] : params) {
    if (!out.empty()) out += '&';
    out += k + '=';
    for (unsigned char c : v) {
      if (std::isalnum(c) || c == '-' || c == '_' || c == '.') out += static_cast<char>(c);
      else if (c == ' ') out += '+';
      else {
        char buf[4];
        std::snprintf(buf, sizeof buf, "%%%02X", c);
        out += buf;
      }
    }
  }
  return out;
}

struct Webhooks {
  std::unique_ptr<AppContext> app;
  CrowApp crow;
  explicit Webhooks(std::map<std::string, std::string> env) : app(test::fakeApp(std::move(env))) {
    seedDemo(*app);
    registerTwilioRoutes(crow, *app);
    crow.validate();
  }
  TwilioProvider* twilio() { return dynamic_cast<TwilioProvider*>(app->voice.get()); }

  // POSTs a form to path_and_query, signed like Twilio would (sign=false sends no signature).
  crow::response post(const std::string& path_and_query, const std::map<std::string, std::string>& params,
                      bool sign = true) {
    crow::request req;
    req.method = crow::HTTPMethod::POST;
    req.raw_url = path_and_query;
    req.url = path_and_query.substr(0, path_and_query.find('?'));
    req.url_params = crow::query_string(path_and_query);
    req.body = formEncode(params);
    req.add_header("Content-Type", "application/x-www-form-urlencoded");
    if (sign && twilio()) req.add_header("X-Twilio-Signature", twilio()->signatureFor(kBase + path_and_query, params));
    crow::response res;
    crow.handle_full(req, res);
    return res;
  }

  void addCall(const std::string& id) { app->repos->upsertCall({id, "u_demo", "CA_TEST", "wake", "ringing", 1, 0}); }
};

}  // namespace

TEST_CASE("validSignature accepts Twilio's documented example and rejects a tampered one") {
  // https://www.twilio.com/docs/usage/webhooks/webhooks-security
  TwilioProvider t(Env::fromMap({{"TWILIO_AUTH_TOKEN", "12345"}}));
  std::string url = "https://mycompany.com/myapp.php?foo=1&bar=2";
  std::map<std::string, std::string> params = {{"CallSid", "CA1234567890ABCDE"},
                                               {"Caller", "+14158675310"},
                                               {"Digits", "1234"},
                                               {"From", "+14158675310"},
                                               {"To", "+18005551212"}};
  CHECK(t.validSignature(url, params, "GvWf1cFY/Q7PnoempGyD5oXAezc="));

  auto tampered = params;
  tampered["Digits"] = "1235";
  CHECK_FALSE(t.validSignature(url, tampered, "GvWf1cFY/Q7PnoempGyD5oXAezc="));
  CHECK_FALSE(t.validSignature(url + "&baz=3", params, "GvWf1cFY/Q7PnoempGyD5oXAezc="));
  CHECK_FALSE(t.validSignature(url, params, ""));
  CHECK_FALSE(TwilioProvider(Env::fromMap({})).validSignature(url, params, "GvWf1cFY/Q7PnoempGyD5oXAezc="));
}

TEST_CASE("base64 and HMAC-SHA1 match RFC 2202 / RFC 4648 vectors") {
  CHECK(twilio::base64("") == "");
  CHECK(twilio::base64("f") == "Zg==");
  CHECK(twilio::base64("fo") == "Zm8=");
  CHECK(twilio::base64("foobar") == "Zm9vYmFy");
  // RFC 2202 test case 2.
  CHECK(twilio::base64(twilio::hmacSha1("Jefe", "what do ya want for nothing?")) == "7/zfauXrL6LSdBbV8YTfnCWafHk=");
}

TEST_CASE("readiness check names every missing setting without throwing") {
  CHECK(TwilioProvider(Env::fromMap({})).readinessProblems().size() == 4);
  auto env = twilioEnv();
  env["PUBLIC_BASE_URL"] = "https://abc.ngrok.app";
  CHECK(TwilioProvider(Env::fromMap(env)).readinessProblems().empty());
  CHECK(TwilioProvider(Env::fromMap(twilioEnv())).readinessProblems().size() == 1);  // http:// base
}

TEST_CASE("form parsing, status mapping, and stop detection") {
  auto f = twilio::parseForm("From=%2B15551234567&Body=hello+there&Empty=");
  CHECK(f["From"] == "+15551234567");
  CHECK(f["Body"] == "hello there");
  CHECK(f["Empty"] == "");
  for (auto s : {"no-answer", "busy", "failed", "canceled"}) CHECK(twilio::eventForStatus(s) == "no_answer");
  CHECK(twilio::eventForStatus("completed") == "completed");
  CHECK(twilio::eventForStatus("ringing") == "");
  CHECK(twilio::isStop("", "0"));
  CHECK(twilio::isStop("Please STOP calling", ""));
  CHECK(twilio::isStop("just hang up", ""));
  CHECK_FALSE(twilio::isStop("I'm up", "1"));
  CHECK_FALSE(twilio::isStop("unstoppable", ""));
}

TEST_CASE("Gather TwiML takes speech or one digit and posts back on silence") {
  std::string g = twiml::gatherSpeech("/twilio/gather?call_id=c&x=1");
  CHECK(g.find("input=\"speech dtmf\"") != std::string::npos);
  CHECK(g.find("numDigits=\"1\"") != std::string::npos);
  CHECK(g.find("actionOnEmptyResult=\"true\"") != std::string::npos);
  CHECK(g.find("call_id=c&amp;x=1") != std::string::npos);
}

TEST_CASE("/twilio/voice renders the opener with Say + Gather, and rejects unsigned requests") {
  Webhooks w(twilioEnv());
  w.addCall("call_t1");
  CHECK(w.post("/twilio/voice?call_id=call_t1", {{"CallSid", "CA_TEST"}}, false).code == 403);

  auto res = w.post("/twilio/voice?call_id=call_t1", {{"CallSid", "CA_TEST"}, {"CallStatus", "in-progress"}});
  CHECK(res.code == 200);
  CHECK(res.body.find("<Gather input=\"speech dtmf\" action=\"/twilio/gather?call_id=call_t1\"") != std::string::npos);
  CHECK(res.body.find("<Say voice=") != std::string::npos);
  CHECK(res.body.find("<Hangup/>") == std::string::npos);
  CHECK(w.app->repos->call("call_t1")->state == "in_progress");

  // Unsigned and pointed at another call: still rejected.
  CHECK(w.post("/twilio/voice?call_id=other", {{"CallSid", "CA_TEST"}}, false).code == 403);
}

TEST_CASE("/twilio/gather: speech goes to the Conversation, keypad 1 means I'm up, keypad 0 hangs up") {
  Webhooks w(twilioEnv());
  w.addCall("call_t2");
  w.post("/twilio/voice?call_id=call_t2", {{"CallSid", "CA_TEST"}});

  auto res = w.post("/twilio/gather?call_id=call_t2", {{"SpeechResult", "five more minutes"}});
  CHECK(res.code == 200);
  CHECK(res.body.find("<Say") != std::string::npos);
  CHECK(w.app->repos->call("call_t2")->turns == 1);

  res = w.post("/twilio/gather?call_id=call_t2", {{"Digits", "1"}});
  CHECK(w.app->repos->call("call_t2")->turns == 2);

  // Silence: no turn recorded, just a reprompt that keeps listening.
  res = w.post("/twilio/gather?call_id=call_t2", {});
  CHECK(res.body.find("Still there?") != std::string::npos);
  CHECK(w.app->repos->call("call_t2")->turns == 2);

  res = w.post("/twilio/gather?call_id=call_t2", {{"Digits", "0"}});
  CHECK(res.body.find("<Hangup/>") != std::string::npos);
  CHECK(res.body.find("<Gather") == std::string::npos);
  CHECK(w.app->repos->call("call_t2")->turns == 2);
}

TEST_CASE("/twilio/gather ends a call that keeps looping") {
  Webhooks w(twilioEnv());
  w.addCall("call_t3");
  w.post("/twilio/voice?call_id=call_t3", {});
  crow::response res;
  for (int i = 0; i < TwilioProvider::kMaxRounds; ++i) res = w.post("/twilio/gather?call_id=call_t3", {});
  CHECK(res.body.find("<Hangup/>") != std::string::npos);
}

TEST_CASE("/twilio/status no-answer emits no_answer") {
  Webhooks w(twilioEnv());
  w.addCall("call_t4");
  CHECK(w.post("/twilio/status?call_id=call_t4", {{"CallStatus", "no-answer"}}, false).code == 403);
  CHECK(w.app->repos->call("call_t4")->state == "ringing");

  CHECK(w.post("/twilio/status?call_id=call_t4", {{"CallStatus", "ringing"}}).code == 204);
  CHECK(w.app->repos->call("call_t4")->state == "ringing");
  CHECK(w.post("/twilio/status?call_id=call_t4", {{"CallStatus", "no-answer"}}).code == 204);
  CHECK(w.app->repos->call("call_t4")->state == "no_answer");

  w.addCall("call_t5");
  w.post("/twilio/status?call_id=call_t5", {{"CallStatus", "completed"}});
  CHECK(w.app->repos->call("call_t5")->state == "completed");
}

TEST_CASE("inbound STOP revokes the contact") {
  auto mom = [](AppContext& app) {
    for (auto& c : app.repos->contacts("u_demo"))
      if (c.phone == "+16195554417") return c.consent;
    return ConsentStatus::PENDING;
  };
  SUBCASE("twilio mode, signed") {
    Webhooks w(twilioEnv());
    REQUIRE(mom(*w.app) == ConsentStatus::CONFIRMED);
    CHECK(w.post("/twilio/sms", {{"From", "+16195554417"}, {"Body", "Stop"}}, false).code == 403);
    CHECK(mom(*w.app) == ConsentStatus::CONFIRMED);
    auto res = w.post("/twilio/sms", {{"From", "+16195554417"}, {"Body", " Stop "}});
    CHECK(res.code == 200);
    CHECK(res.body.find("<Response/>") != std::string::npos);
    CHECK(mom(*w.app) == ConsentStatus::REVOKED);
  }
  SUBCASE("sim mode stays permissive") {
    Webhooks w({});
    w.post("/twilio/sms", {{"From", "+16195554417"}, {"Body", "hello"}});
    CHECK(mom(*w.app) == ConsentStatus::CONFIRMED);
    w.post("/twilio/sms", {{"From", "+16195554417"}, {"Body", "STOP"}});
    CHECK(mom(*w.app) == ConsentStatus::REVOKED);
  }
}

TEST_CASE("say and hangup inside a webhook turn are rendered, not sent over REST") {
  TwilioProvider t(Env::fromMap(twilioEnv()));
  t.beginTurn("c");
  t.say("c", "Morning!", "best_friend", Urgency::CHILL, "");
  t.hangup("c", "completed");
  auto out = t.endTurn("c");
  REQUIRE(out.lines.size() == 1);
  CHECK(out.lines[0].text == "Morning!");
  CHECK(out.hangup);
  CHECK(out.round == 1);
  // Outside a turn there's no CallSid and no valid config: hangup is a logged no-op, not a crash.
  t.hangup("c", "completed");
  CHECK(t.placeCall({"c2", "u", "+15551230000", "wake", 1, "best_friend", Urgency::CHILL}) == "TWILIO-UNCONFIGURED-c2");
}

TEST_CASE("a browser subscribed by user id rings for every call to that user") {
  SimProvider sim;
  std::vector<std::string> events;
  sim.onCallEvent([&](const std::string& id, const std::string& ev, const std::string&) { events.push_back(id + ":" + ev); });
  std::vector<std::string> got;
  int conn = 0;
  sim.attachBrowser("u_demo", &conn, [&](const std::string& t) { got.push_back(t); });
  sim.placeCall({"call_a", "u_demo", "+1", "wake", 1, "best_friend", Urgency::CHILL});
  sim.placeCall({"call_b", "u_demo", "+1", "wake", 2, "best_friend", Urgency::FIRM});
  sim.placeCall({"call_c", "someone_else", "+1", "wake", 1, "best_friend", Urgency::CHILL});
  REQUIRE(got.size() == 2);
  CHECK(nlohmann::json::parse(got[1])["data"]["call_id"] == "call_b");
  // Replies on the user socket carry their call id in data.call_id.
  sim.onBrowserMessage("u_demo", R"({"type":"decline","ts":0,"data":{"call_id":"call_b"}})");
  CHECK(events == std::vector<std::string>{"call_b:declined"});
}
