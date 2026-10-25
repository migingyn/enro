// Twilio telephony (VOICE_PROVIDER=twilio). Outbound calls via the REST API, call flow via
// TwiML webhooks in http/TwilioRoutes.cpp, voice-in via <Gather input="speech dtmf">. Owner: T02.
//
// Twilio speaks through the TwiML we return, so say() and hangup() during a webhook don't hit the
// API: TwilioRoutes opens a turn (beginTurn), emits the event, and renders whatever the
// Conversation queued (endTurn). Outside a webhook, hangup() updates the live call over REST.
#pragma once

#include <map>
#include <mutex>
#include <string>
#include <vector>

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

  // Validates X-Twilio-Signature: base64(HMAC-SHA1(auth_token, url + sorted key+value pairs)).
  // url is the full public URL Twilio called (PUBLIC_BASE_URL + path + query).
  bool validSignature(const std::string& url, const std::map<std::string, std::string>& params,
                      const std::string& signature) const;
  std::string signatureFor(const std::string& url, const std::map<std::string, std::string>& params) const;

  // Empty when every setting a live call needs looks right, else what's wrong (one line each).
  std::vector<std::string> readinessProblems() const;
  const std::string& publicBaseUrl() const { return public_base_url_; }

  // Called by TwilioRoutes when a webhook reports a call event; forwards to the CallEventHandler.
  void handleWebhookEvent(const std::string& call_id, const std::string& event, const std::string& text) {
    emit(call_id, event, text);
  }

  // One webhook round trip. Lines and hangups queued between beginTurn and endTurn are rendered as TwiML.
  struct Spoken {
    std::string text, audio_url, persona;
  };
  struct TurnOutput {
    std::vector<Spoken> lines;
    bool hangup = false;
    int round = 0;               // 1 on the first webhook of a call
    long long elapsed_ms = 0;    // since the first webhook
  };
  // beginTurn returns only round and elapsed_ms (to check the loop bounds before emitting).
  TurnOutput beginTurn(const std::string& call_id);
  TurnOutput endTurn(const std::string& call_id);
  void forget(const std::string& call_id);  // call ended: drop its state

  // Prompt loop bounds so a call can't loop forever.
  static constexpr int kMaxRounds = 7;
  static constexpr long long kMaxCallMs = 110'000;

 private:
  struct LiveCall {
    std::string sid;
    int rounds = 0;
    long long started_ms = 0;
    bool in_turn = false;
    bool hangup = false;
    std::vector<Spoken> queued;
  };
  void hangupOverRest(const std::string& call_id, const std::string& sid);

  std::string account_sid_, auth_token_, from_number_, public_base_url_;
  mutable std::mutex mu_;
  std::map<std::string, LiveCall> calls_;  // call_id -> state
};

// Helpers shared with the routes and tests.
namespace twilio {
std::string base64(const std::string& bytes);
std::string hmacSha1(const std::string& key, const std::string& message);  // raw 20-byte digest
std::map<std::string, std::string> parseForm(const std::string& body);    // application/x-www-form-urlencoded
// "no_answer" | "completed" | "" (ignore) for a Twilio CallStatus.
std::string eventForStatus(const std::string& call_status);
// Keypad 0 or "stop / cancel / hang up" ends the call before anything else is considered.
bool isStop(const std::string& speech, const std::string& digits);
}  // namespace twilio

}  // namespace enro
