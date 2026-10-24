// CLAUDE API ALTERNATE
// Switch with LLM_PROVIDER=anthropic and uncomment this file + AnthropicClient.cpp
// + the `anthropic` branch in app/App.cpp (makeLlm). CMake already globs the .cpp.
// Covers text only. TTS, transcription and Realtime onboarding stay on OpenAI (PRD 8.1).
//
// #pragma once
//
// #include "ai/LlmClient.hpp"
// #include "config/Env.hpp"
//
// namespace enro {
//
// class AnthropicClient : public LlmClient {
//  public:
//   explicit AnthropicClient(const Env& env);
//   std::string name() const override { return "anthropic"; }
//   nlohmann::json chatJson(const std::string& system, const std::string& user, const nlohmann::json& schema) override;
//   std::string chat(const std::string& system, const std::vector<ChatMessage>& messages) override;
//
//  private:
//   std::string api_key_;  // ANTHROPIC_API_KEY
//   std::string model_;    // ANTHROPIC_MODEL
// };
//
// }  // namespace enro
