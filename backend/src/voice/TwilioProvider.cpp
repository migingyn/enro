#include "voice/TwilioProvider.hpp"

#include "util/Log.hpp"

namespace enro {

TwilioProvider::TwilioProvider(const Env& env)
    : account_sid_(env.get("TWILIO_ACCOUNT_SID")),
      auth_token_(env.get("TWILIO_AUTH_TOKEN")),
      from_number_(env.get("TWILIO_FROM_NUMBER")),
      public_base_url_(env.get("PUBLIC_BASE_URL")) {
  if (account_sid_.empty() || auth_token_.empty()) LOG_WARN("twilio", "VOICE_PROVIDER=twilio but credentials are empty");
}

std::string TwilioProvider::placeCall(const CallRequest& req) {
  // STUB
  // TODO(T02): POST /2010-04-01/Accounts/{sid}/Calls.json with Url=<PUBLIC_BASE_URL>/twilio/voice?call_id=...,
  // StatusCallback=/twilio/status. Return the CallSid.
  LOG_WARN("twilio", "placeCall not implemented yet (T02), call " << req.call_id << " not placed");
  return "TWILIO-STUB-" + req.call_id;
}

void TwilioProvider::say(const std::string&, const std::string&, const std::string&, Urgency, const std::string&) {
  // STUB
  // TODO(T02): Twilio speaks through TwiML returned by the webhooks. Queue the line for the next
  // /twilio/voice or /twilio/gather response, or update the live call with new TwiML.
}

void TwilioProvider::hangup(const std::string&, const std::string&) {
  // STUB
  // TODO(T02): POST Calls/{sid}.json Status=completed.
}

std::string TwilioProvider::sendSms(const std::string& to_phone, const std::string&) {
  // STUB
  // TODO(T02): POST Messages.json From=TWILIO_FROM_NUMBER To=to_phone Body=body. Return the MessageSid.
  LOG_WARN("twilio", "sendSms not implemented yet (T02), nothing sent to " << to_phone);
  return "TWILIO-STUB-SMS";
}

bool TwilioProvider::validSignature(const std::string&, const std::map<std::string, std::string>&,
                                    const std::string&) const {
  // STUB
  // TODO(T02): validate X-Twilio-Signature per Twilio docs. Reject unsigned webhooks when VOICE_PROVIDER=twilio.
  return true;
}

}  // namespace enro
