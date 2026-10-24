// RETELL ALTERNATE (keep commented)
// Switch with VOICE_PROVIDER=retell. Retell owns STT, TTS, and turn-taking.
// Outbound via Retell create-phone-call with dynamic variables (persona, urgency,
// plan, consequence summary). Custom LLM websocket at /retell/llm/:call_id runs
// IntentClassifier + LineGenerator. Webhook at /retell/webhook.
// Check current Retell docs before enabling. Also uncomment registerRetellRoutes in Routes.hpp/.cpp.
//
// #include "app/App.hpp"
// #include "http/Routes.hpp"
//
// namespace enro {
//
// void registerRetellRoutes(CrowApp& app, AppContext& ctx) {
//   // Retell sends each user utterance; reply with the agent's next line.
//   CROW_WEBSOCKET_ROUTE(app, "/retell/llm/<string>")
//       .onmessage([&ctx](crow::websocket::connection& conn, const std::string& data, bool) {
//         // parse { interaction_type, transcript, response_id }; on "response_required":
//         // ctx.conversation->onEvent(call_id, "speech", last_user_text) and send
//         // { response_id, content: line.text, content_complete: true, end_call: line.end_call }.
//       });
//
//   // Call lifecycle: call_started, call_ended, call_analyzed.
//   CROW_ROUTE(app, "/retell/webhook").methods(crow::HTTPMethod::POST)([&ctx](const crow::request& req) {
//     // map call_ended with no user turns -> ctx.conversation->onEvent(call_id, "no_answer", "")
//     return crow::response(204);
//   });
// }
//
// }  // namespace enro
