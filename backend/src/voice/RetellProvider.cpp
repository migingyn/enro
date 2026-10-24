// RETELL ALTERNATE (keep commented)
// Switch with VOICE_PROVIDER=retell. Retell owns STT, TTS, and turn-taking.
// Outbound via Retell create-phone-call with dynamic variables (persona, urgency,
// plan, consequence summary). Custom LLM websocket at /retell/llm/:call_id runs
// IntentClassifier + LineGenerator. Webhook at /retell/webhook.
// Check current Retell docs before enabling.
//
// #include "voice/RetellProvider.hpp"
//
// #include <cpr/cpr.h>
//
// namespace enro {
//
// RetellProvider::RetellProvider(const Env& env)
//     : api_key_(env.get("RETELL_API_KEY")), agent_id_(env.get("RETELL_AGENT_ID")) {}
//
// std::string RetellProvider::placeCall(const CallRequest& req) {
//   // POST create-phone-call with from/to numbers, agent id, and retell_llm_dynamic_variables:
//   // { persona, urgency, plan, consequence_summary }. Return the Retell call_id.
//   return "";
// }
//
// void RetellProvider::say(const std::string&, const std::string&, const std::string&, Urgency, const std::string&) {
//   // Retell speaks whatever the custom LLM websocket returns, so lines are sent from RetellRoutes.
// }
//
// void RetellProvider::hangup(const std::string&, const std::string&) {
//   // End the call through the custom LLM websocket (end_call: true).
// }
//
// std::string RetellProvider::sendSms(const std::string& to_phone, const std::string& body) {
//   // Retell does not cover SMS. Keep Twilio for texts.
//   return "";
// }
//
// }  // namespace enro
