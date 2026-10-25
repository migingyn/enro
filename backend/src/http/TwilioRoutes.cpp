// Twilio webhooks (PRD 11). Validate X-Twilio-Signature on every route. Owner: T02.
// Expected TwiML shapes: contracts/examples/twiml/*.xml.
//
// With VOICE_PROVIDER=twilio every webhook must carry a valid signature over
// PUBLIC_BASE_URL + path + query and the POST params, or it gets a 403. In sim mode the routes stay
// permissive (no Twilio account to sign with) and answer with the fixture TwiML.
#include <algorithm>
#include <cctype>

#include "app/App.hpp"
#include "http/Routes.hpp"
#include "util/Log.hpp"
#include "voice/TwiML.hpp"
#include "voice/TwilioProvider.hpp"

namespace enro {

namespace {

TwilioProvider* twilioOf(AppContext& ctx) { return dynamic_cast<TwilioProvider*>(ctx.voice.get()); }

// false when VOICE_PROVIDER=twilio and the request isn't signed by our auth token.
bool signedByTwilio(AppContext& ctx, const crow::request& req, const std::map<std::string, std::string>& params) {
  auto* tw = twilioOf(ctx);
  if (!tw) return true;
  if (tw->validSignature(tw->publicBaseUrl() + req.raw_url, params, req.get_header_value("X-Twilio-Signature")))
    return true;
  LOG_WARN("twilio", "rejected unsigned or forged webhook " << req.url);
  return false;
}

std::string callIdOf(const crow::request& req) {
  const char* id = req.url_params.get("call_id");
  return id ? id : "";
}

std::string gatherUrl(const std::string& call_id) { return "/twilio/gather?call_id=" + call_id; }

std::string finish(const std::string& line) {
  return twiml::response(twiml::say(line, twiml::voiceFor("")) + twiml::hangup());
}

// What the Conversation said this turn, then listen again (or hang up if it ended the call).
std::string render(const TwilioProvider::TurnOutput& out, const std::string& call_id, const std::string& reprompt) {
  std::string lines;
  for (auto& l : out.lines)
    lines += l.audio_url.empty() ? twiml::say(l.text, twiml::voiceFor(l.persona)) : twiml::play(l.audio_url);
  if (out.hangup) return twiml::response(lines + twiml::hangup());
  if (lines.empty()) lines = twiml::say(reprompt, twiml::voiceFor(""));
  // Lines go inside the Gather so the user can barge in ("five more minutes", or 0 to stop).
  return twiml::response(twiml::gatherSpeech(gatherUrl(call_id), lines));
}

bool overLimit(const TwilioProvider::TurnOutput& t) {
  return t.round > TwilioProvider::kMaxRounds || t.elapsed_ms > TwilioProvider::kMaxCallMs;
}

std::string upperTrim(std::string s) {
  s.erase(0, s.find_first_not_of(" \t\r\n"));
  s.erase(s.find_last_not_of(" \t\r\n.!") + 1);
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::toupper(c); });
  return s;
}

}  // namespace

void registerTwilioRoutes(CrowApp& app, AppContext& ctx) {
  // Call connected: the Conversation produces the opener, then <Gather input="speech dtmf">.
  CROW_ROUTE(app, "/twilio/voice").methods(crow::HTTPMethod::POST)([&ctx](const crow::request& req) {
    auto params = twilio::parseForm(req.body);
    if (!signedByTwilio(ctx, req, params)) return crow::response(403);
    auto* tw = twilioOf(ctx);
    if (!tw) return xmlResponse(ctx.fixtures.text("twiml/voice.xml"));  // sim mode
    std::string call_id = callIdOf(req);
    if (call_id.empty()) return xmlResponse(finish("This is Enro. Something went wrong on our side. Goodbye."));

    tw->beginTurn(call_id);
    tw->handleWebhookEvent(call_id, "answered", "");
    auto out = tw->endTurn(call_id);
    if (out.lines.empty() && !out.hangup) return xmlResponse(finish("This is Enro. I lost track of this call. Goodbye."));
    return xmlResponse(render(out, call_id, "Still there? Say I'm up, or press 1."));
  });

  // Speech or keypad result from <Gather> (also posted on silence: actionOnEmptyResult).
  CROW_ROUTE(app, "/twilio/gather").methods(crow::HTTPMethod::POST)([&ctx](const crow::request& req) {
    auto params = twilio::parseForm(req.body);
    if (!signedByTwilio(ctx, req, params)) return crow::response(403);
    auto* tw = twilioOf(ctx);
    if (!tw) return xmlResponse(ctx.fixtures.text("twiml/gather.xml"));  // sim mode
    std::string call_id = callIdOf(req);
    std::string speech = params.count("SpeechResult") ? params["SpeechResult"] : "";
    std::string digits = params.count("Digits") ? params["Digits"] : "";

    // Stop beats everything else, including the loop bounds and the Conversation.
    if (twilio::isStop(speech, digits)) {
      LOG_INFO("twilio", call_id << " stopped by the user");
      return xmlResponse(finish("Okay. Ending your call."));
    }
    auto turn = tw->beginTurn(call_id);
    if (overLimit(turn)) {
      tw->endTurn(call_id);
      LOG_INFO("twilio", call_id << " hit the prompt limit after " << turn.round - 1 << " rounds");
      return xmlResponse(finish("I have to go now. Call me back when you're up."));
    }
    if (digits == "1") speech = "I am up";
    if (!speech.empty()) tw->handleWebhookEvent(call_id, "speech", speech);
    auto out = tw->endTurn(call_id);
    return xmlResponse(render(out, call_id, "Still there? Say I'm up, or press 1. Press 0 to stop."));
  });

  // Call status callback: initiated, ringing, answered, completed (and no-answer, busy, failed, canceled).
  CROW_ROUTE(app, "/twilio/status").methods(crow::HTTPMethod::POST)([&ctx](const crow::request& req) {
    auto params = twilio::parseForm(req.body);
    if (!signedByTwilio(ctx, req, params)) return crow::response(403);
    auto* tw = twilioOf(ctx);
    std::string call_id = callIdOf(req);
    std::string status = params.count("CallStatus") ? params["CallStatus"] : "";
    std::string event = twilio::eventForStatus(status);
    LOG_INFO("twilio", "status " << call_id << ": " << status);
    if (tw && !call_id.empty() && !event.empty()) {
      tw->handleWebhookEvent(call_id, event, "");
      tw->forget(call_id);
    }
    return crow::response(204);
  });

  // Inbound SMS (a contact replies STOP). Works in sim mode too.
  CROW_ROUTE(app, "/twilio/sms").methods(crow::HTTPMethod::POST)([&ctx](const crow::request& req) {
    auto params = twilio::parseForm(req.body);
    if (!signedByTwilio(ctx, req, params)) return crow::response(403);
    std::string body = upperTrim(params.count("Body") ? params["Body"] : "");
    std::string from = params.count("From") ? params["From"] : "";
    if (!from.empty() && (body == "STOP" || body == "STOPALL" || body == "UNSUBSCRIBE" || body == "CANCEL" ||
                          body == "END" || body == "QUIT"))
      ctx.contacts->revokeByPhone(from);
    return xmlResponse(ctx.fixtures.text("twiml/sms.xml"));
  });
}

}  // namespace enro
