// Twilio webhooks (PRD 11). Validate X-Twilio-Signature on every route. Owner: T02.
// Expected TwiML shapes: contracts/examples/twiml/*.xml.
#include "app/App.hpp"
#include "http/Routes.hpp"

namespace enro {

void registerTwilioRoutes(CrowApp& app, AppContext& ctx) {
  // Call connected: speak the opener, then <Gather input="speech">.
  CROW_ROUTE(app, "/twilio/voice").methods(crow::HTTPMethod::POST)([&ctx](const crow::request&) {
    // STUB
    // TODO(T02): validate signature; call_id from the query; emit "answered" through TwilioProvider so the
    // Conversation produces the opener; render it with twiml::play(AudioCache URL) or twiml::say + gatherSpeech.
    return xmlResponse(ctx.fixtures.text("twiml/voice.xml"));
  });

  // Speech result from <Gather>.
  CROW_ROUTE(app, "/twilio/gather").methods(crow::HTTPMethod::POST)([&ctx](const crow::request&) {
    // STUB
    // TODO(T02): validate signature; SpeechResult -> emit "speech"; render the reply + another gather (or hangup).
    return xmlResponse(ctx.fixtures.text("twiml/gather.xml"));
  });

  // Call status callback: ringing, no-answer, busy, completed.
  CROW_ROUTE(app, "/twilio/status").methods(crow::HTTPMethod::POST)([](const crow::request&) {
    // STUB
    // TODO(T02): validate signature; map CallStatus no-answer/busy/failed -> "no_answer", completed -> "completed".
    return crow::response(204);
  });

  // Inbound SMS (a contact replies STOP).
  CROW_ROUTE(app, "/twilio/sms").methods(crow::HTTPMethod::POST)([&ctx](const crow::request&) {
    // STUB
    // TODO(T02): validate signature; Body STOP -> ctx.contacts->revokeByPhone(From).
    return xmlResponse(ctx.fixtures.text("twiml/sms.xml"));
  });
}

}  // namespace enro
