// Twilio telephony (VOICE_PROVIDER=twilio). Outbound calls via the REST API, call flow via
// TwiML webhooks in http/TwilioRoutes.cpp, voice-in via <Gather input="speech">. Owner: T02.
#pragma once

#include "config/Env.hpp"
#include "voice/VoiceProvider.hpp"

namespace enro {

class TwilioProvider : public VoiceProvider {
 public:
  explicit TwilioProvider(const Env& env);

  std::string name() const override { return "twilio"; }

  std::string placeCall(const CallRequest& req) override;
  void say(const std::string& call_id, const std::string& text, const std::string& persona, Urgency urgency,
           const std::string& audio_url) override;
  void hangup(const std::string& call_id, const std::string& reason) override;
  std::string sendSms(const std::string& to_phone, const std::string& body) override;

  // Validates X-Twilio-Signature (HMAC-SHA1 of URL + sorted POST params with the auth token).
  bool validSignature(const std::string& url, const std::map<std::string, std::string>& params,
                      const std::string& signature) const;

  // Called by TwilioRoutes when a webhook reports a call event; forwards to the CallEventHandler.
  void handleWebhookEvent(const std::string& call_id, const std::string& event, const std::string& text) {
    emit(call_id, event, text);
  }

 private:
  std::string account_sid_, auth_token_, from_number_, public_base_url_;
};

}  // namespace enro
