// AppContext: every provider and service, built once at boot from env (PRD 12, 15.2).
// Provider selection lives in App.cpp. Shared file (owner: Mikey); tasks shouldn't need to edit it
// except to wire a new dependency.
#pragma once

#include <memory>
#include <string>

#include <nlohmann/json.hpp>

#include "ai/IntentClassifier.hpp"
#include "ai/LineGenerator.hpp"
#include "ai/LlmClient.hpp"
#include "ai/SpeechClient.hpp"
#include "audio/AudioCache.hpp"
#include "calls/Conversation.hpp"
#include "calls/WakeLadder.hpp"
#include "config/Env.hpp"
#include "config/Fixtures.hpp"
#include "contacts/ContactService.hpp"
#include "google/CalendarSource.hpp"
#include "google/EmailSource.hpp"
#include "google/GoogleAuth.hpp"
#include "http/DashboardSocket.hpp"
#include "model/PunctualityModel.hpp"
#include "model/TransportLearner.hpp"
#include "motion/MotionTracker.hpp"
#include "planner/ConsequenceEngine.hpp"
#include "planner/ReadyToLeave.hpp"
#include "planner/Urgency.hpp"
#include "reflex/Reflex.hpp"
#include "cache/CachedRouter.hpp"
#include "routing/Router.hpp"
#include "scheduler/Scheduler.hpp"
#include "scheduler/TaskQueue.hpp"
#include "sim/LoadSimulator.hpp"
#include "store/Database.hpp"
#include "store/Repos.hpp"
#include "transit/Commentator.hpp"
#include "transit/TransitFeed.hpp"
#include "transit/TripRunner.hpp"
#include "triage/EmailTriage.hpp"
#include "voice/SimProvider.hpp"
#include "voice/VoiceProvider.hpp"

namespace enro {

struct ProviderNames {
  std::string voice, llm, speech, reflex, calendar, email, router, transit;
};

struct AppContext {
  explicit AppContext(Env e);
  ~AppContext();

  Env env;
  Fixtures fixtures;
  long long started_at = 0;
  ProviderNames providers;

  // backend/config/*.json (empty objects when missing)
  nlohmann::json event_types, personas, scripts, onboarding;

  // infrastructure
  std::unique_ptr<Database> db;
  std::unique_ptr<Repos> repos;
  std::unique_ptr<DashboardSocket> dashboard;

  // providers (fake by default)
  std::shared_ptr<LlmClient> llm;
  std::shared_ptr<SpeechClient> speech;
  std::unique_ptr<VoiceProvider> voice;
  SimProvider* sim = nullptr;  // == voice when VOICE_PROVIDER=sim, else null
  std::unique_ptr<Reflex> reflex;
  std::unique_ptr<GoogleAuth> google_auth;
  std::unique_ptr<CalendarSource> calendar;
  std::unique_ptr<EmailSource> email;
  std::unique_ptr<FallbackRouter> fallback_router;
  std::unique_ptr<Router> google_router;
  std::unique_ptr<CachedRouter> cached_router;  // LRU in front of google_router or fallback_router
  Router* router = nullptr;  // cached_router, wrapping google_router when GOOGLE_MAPS_API_KEY is set, else fallback_router
  std::unique_ptr<TransitFeed> makeTransitFeed() const;  // a fresh feed per trip

  // services
  std::unique_ptr<UrgencyScorer> urgency;
  std::unique_ptr<ReadyToLeave> ready_to_leave;
  std::unique_ptr<ConsequenceEngine> consequences;
  std::unique_ptr<PunctualityModel> punctuality;
  std::unique_ptr<TransportLearner> transport;
  std::unique_ptr<TaskQueue> queue;
  std::unique_ptr<Scheduler> scheduler;
  std::unique_ptr<IntentClassifier> intents;
  std::unique_ptr<LineGenerator> lines;
  std::unique_ptr<EmailTriage> triage;
  std::unique_ptr<WakeLadder> wake_ladder;
  std::unique_ptr<Conversation> conversation;
  std::unique_ptr<ContactService> contacts;
  std::unique_ptr<Commentator> commentator;
  std::unique_ptr<TripRunner> trip_runner;
  std::unique_ptr<LoadSimulator> load_sim;
  std::unique_ptr<AudioCache> audio;
  std::unique_ptr<MotionTracker> motion;  // Sensor Logger phone motion (S7)

  // ---- dashboard helpers: persist + broadcast in one call ----
  // Every agent action writes a Decision (PRD 7.1).
  void recordDecision(const Decision& d);
  void recordDecision(const std::string& user_id, const std::string& kind, const ReflexVerdict& v,
                      const nlohmann::json& inputs);
  void updateCall(const Call& c);  // upsert + broadcast call_state
  void recordTurn(const Turn& t);  // insert + broadcast turn
};

// Builds everything from env. Never throws for missing keys: unknown or unconfigured providers
// fall back to the fake one with a warning.
std::unique_ptr<AppContext> buildApp(Env env);

}  // namespace enro
