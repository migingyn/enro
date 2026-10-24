// RETELL ALTERNATE (keep commented)
// Switch with VOICE_PROVIDER=retell. Retell owns STT, TTS, and turn-taking.
// Outbound via Retell create-phone-call with dynamic variables (persona, urgency,
// plan, consequence summary). Custom LLM websocket at /retell/llm/:call_id runs
// IntentClassifier + LineGenerator. Webhook at /retell/webhook.
// Check current Retell docs before enabling.
// Uncomment this file, RetellProvider.cpp, http/RetellRoutes.cpp, and the `retell` branch in app/App.cpp.
//
// #pragma once
//
// #include "config/Env.hpp"
// #include "voice/VoiceProvider.hpp"
//
// namespace enro {
//
// class RetellProvider : public VoiceProvider {
//  public:
//   explicit RetellProvider(const Env& env);  // RETELL_API_KEY, RETELL_AGENT_ID
//   std::string name() const override { return "retell"; }
//   std::string placeCall(const CallRequest& req) override;
//   void say(const std::string& call_id, const std::string& text, const std::string& persona, Urgency urgency,
//            const std::string& audio_url) override;
//   void hangup(const std::string& call_id, const std::string& reason) override;
//   std::string sendSms(const std::string& to_phone, const std::string& body) override;
//
//  private:
//   std::string api_key_, agent_id_;
// };
//
// }  // namespace enro
