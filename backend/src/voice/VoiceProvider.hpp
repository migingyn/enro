// Telephony interface. Selected by VOICE_PROVIDER: sim | twilio | retell (retell is commented out).
// The provider places calls, speaks lines, and reports what the user did through CallEventHandler.
#pragma once

#include <functional>
#include <string>

#include "models/Models.hpp"

namespace enro {

struct CallRequest {
  std::string call_id, user_id, to_phone;
  std::string purpose;  // wake | awake_verify | leave_reminder | departure_check
  int attempt = 1;
  std::string persona;
  Urgency urgency = Urgency::CHILL;
};

// event: "answered" | "speech" | "declined" | "no_answer" | "completed". text is set for "speech".
using CallEventHandler =
    std::function<void(const std::string& call_id, const std::string& event, const std::string& text)>;

class VoiceProvider {
 public:
  virtual ~VoiceProvider() = default;
  virtual std::string name() const = 0;

  // Starts ringing. Returns the provider's id for the call (Twilio CallSid, SIM-<call_id>, ...).
  virtual std::string placeCall(const CallRequest& req) = 0;
  // Speak a line on a live call. audio_url is an /audio/<id>.mp3 URL or empty (provider uses its own TTS).
  virtual void say(const std::string& call_id, const std::string& text, const std::string& persona, Urgency urgency,
                   const std::string& audio_url) = 0;
  virtual void hangup(const std::string& call_id, const std::string& reason) = 0;
  // Returns the provider's message id.
  virtual std::string sendSms(const std::string& to_phone, const std::string& body) = 0;

  void onCallEvent(CallEventHandler handler) { handler_ = std::move(handler); }

 protected:
  void emit(const std::string& call_id, const std::string& event, const std::string& text = "") {
    if (handler_) handler_(call_id, event, text);
  }

 private:
  CallEventHandler handler_;
};

}  // namespace enro
