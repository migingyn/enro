#include "app/App.hpp"

#include "ai/AnthropicClient.hpp"
#include "ai/FakeLlm.hpp"
#include "ai/OpenAIClient.hpp"
#include "google/CalendarClient.hpp"
#include "google/GmailClient.hpp"
#include "google/SeedSources.hpp"
#include "models/Json.hpp"
#include "reflex/JevReflex.hpp"
#include "reflex/RulesReflex.hpp"
#include "util/Log.hpp"
#include "util/Time.hpp"
#include "voice/TwilioProvider.hpp"
// #include "voice/RetellProvider.hpp"  // RETELL ALTERNATE

namespace enro {

namespace {

nlohmann::json readConfig(const Env& env, const std::string& name) {
  auto j = Fixtures::readJsonFile(env.configDir() + "/" + name);
  if (!j) LOG_WARN("config", "backend/config/" << name << " missing, using defaults");
  return j.value_or(nlohmann::json::object());
}

void unknownProvider(const std::string& var, const std::string& value, const std::string& fallback) {
  LOG_WARN("providers", var << "=" << value << " is not available, using " << fallback);
}

// ---------- provider selection (PRD 12) ----------

void selectLlm(AppContext& ctx) {
  std::string want = ctx.env.get("LLM_PROVIDER", "fake");
  auto fake = std::make_shared<FakeLlm>();
  ctx.llm = fake;
  ctx.speech = fake;
  if (want == "openai") {
    auto openai = std::make_shared<OpenAIClient>(ctx.env);
    ctx.llm = openai;
    ctx.speech = openai;
  } else if (want == "anthropic") {
    ctx.llm = std::make_shared<AnthropicClient>(ctx.env);
    // Voice always stays on OpenAI (Claude has no audio output).
    if (ctx.env.has("OPENAI_API_KEY")) ctx.speech = std::make_shared<OpenAIClient>(ctx.env);
  } else if (want != "fake") {
    unknownProvider("LLM_PROVIDER", want, "fake");
  }
  ctx.providers.llm = ctx.llm->name();
  ctx.providers.speech = ctx.speech->name();
}

void selectVoice(AppContext& ctx) {
  std::string want = ctx.env.get("VOICE_PROVIDER", "sim");
  if (want == "twilio") {
    ctx.voice = std::make_unique<TwilioProvider>(ctx.env);
  } else {
    if (want == "retell") unknownProvider("VOICE_PROVIDER", want, "sim (RetellProvider is commented out)");
    else if (want != "sim") unknownProvider("VOICE_PROVIDER", want, "sim");
    auto sim = std::make_unique<SimProvider>();
    ctx.sim = sim.get();
    ctx.voice = std::move(sim);
  }
  ctx.providers.voice = ctx.voice->name();
}

void selectReflex(AppContext& ctx) {
  std::string want = ctx.env.get("REFLEX_PROVIDER", "rules");
  if (want == "jev") ctx.reflex = std::make_unique<JevReflex>(ctx.env);
  else {
    if (want != "rules") unknownProvider("REFLEX_PROVIDER", want, "rules");
    ctx.reflex = std::make_unique<RulesReflex>();
  }
  ctx.providers.reflex = ctx.reflex->name();
}

void selectSources(AppContext& ctx) {
  ctx.google_auth = std::make_unique<GoogleAuth>(ctx.env, *ctx.repos);

  std::string cal = ctx.env.get("CALENDAR_SOURCE", "seed");
  if (cal == "google") ctx.calendar = std::make_unique<CalendarClient>(*ctx.google_auth, *ctx.repos);
  else {
    if (cal != "seed") unknownProvider("CALENDAR_SOURCE", cal, "seed");
    ctx.calendar = std::make_unique<SeedCalendar>(ctx.env.seedDir(), ctx.fixtures);
  }

  std::string mail = ctx.env.get("EMAIL_SOURCE", "seed");
  if (mail == "gmail") ctx.email = std::make_unique<GmailClient>(*ctx.google_auth, *ctx.repos);
  else {
    if (mail != "seed") unknownProvider("EMAIL_SOURCE", mail, "seed");
    ctx.email = std::make_unique<SeedEmail>(ctx.env.seedDir());
  }
  ctx.providers.calendar = ctx.calendar->name();
  ctx.providers.email = ctx.email->name();
}

void selectRouter(AppContext& ctx) {
  std::map<std::string, double> speeds;
  if (ctx.event_types.contains("mode_speeds_kmh")) speeds = ctx.event_types["mode_speeds_kmh"].get<std::map<std::string, double>>();
  ctx.fallback_router = std::make_unique<FallbackRouter>(speeds);
  ctx.router = ctx.fallback_router.get();
  if (ctx.env.has("GOOGLE_MAPS_API_KEY")) {
    ctx.google_router = std::make_unique<GoogleRouter>(ctx.env, *ctx.fallback_router);
    ctx.router = ctx.google_router.get();
  }
  ctx.providers.router = ctx.router->name();
  // Every plan asks for the same few trips, so an LRU in front of the router saves Routes API calls (T05).
  ctx.cached_router = std::make_unique<CachedRouter>(*ctx.router);
  ctx.router = ctx.cached_router.get();
  ctx.providers.transit = ctx.makeTransitFeed()->name();
}

}  // namespace

AppContext::AppContext(Env e) : env(std::move(e)), fixtures(env.contractsDir()), started_at(time::nowTs()) {}

AppContext::~AppContext() {
  if (trip_runner) trip_runner->stop();
  if (scheduler) scheduler->stop();
}

std::unique_ptr<TransitFeed> AppContext::makeTransitFeed() const {
  if (env.get("TRANSIT_SOURCE", "replay") == "gtfs") return std::make_unique<GtfsRtFeed>(env);
  return std::make_unique<ReplayFeed>(env.seedDir());
}

void AppContext::recordDecision(const Decision& d) {
  repos->insertDecision(d);
  dashboard->broadcast("decision", d);
}

void AppContext::recordDecision(const std::string& user_id, const std::string& kind, const ReflexVerdict& v,
                                const nlohmann::json& inputs) {
  recordDecision(Decision{user_id, kind, v.verdict, v.source, v.confidence, time::nowTs(), v.latency_ms, inputs});
}

void AppContext::updateCall(const Call& c) {
  repos->upsertCall(c);
  dashboard->broadcast("call_state", c);
}

void AppContext::recordTurn(const Turn& t) {
  repos->insertTurn(t);
  dashboard->broadcast("turn", t);
}

std::unique_ptr<AppContext> buildApp(Env env) {
  auto ctx = std::make_unique<AppContext>(std::move(env));
  AppContext& c = *ctx;

  c.event_types = readConfig(c.env, "event_types.json");
  c.personas = readConfig(c.env, "personas.json");
  c.scripts = readConfig(c.env, "scripts.json");
  c.onboarding = readConfig(c.env, "onboarding.json");
  c.places = std::make_unique<Places>(Places::fromConfig(readConfig(c.env, "places.json")));

  c.db = std::make_unique<Database>(c.env.dbPath());
  c.db->createSchema();
  c.repos = std::make_unique<Repos>(*c.db);
  c.dashboard = std::make_unique<DashboardSocket>();

  selectLlm(c);
  selectVoice(c);
  selectReflex(c);
  selectSources(c);
  selectRouter(c);

  c.urgency = std::make_unique<UrgencyScorer>(UrgencyWeights::fromConfig(c.event_types));
  c.ready_to_leave = std::make_unique<ReadyToLeave>(c.event_types, *c.router, *c.urgency);
  c.consequences = std::make_unique<ConsequenceEngine>();
  c.transport = std::make_unique<TransportLearner>();
  c.punctuality = std::make_unique<PunctualityModel>(c.transport.get());
  c.queue = std::make_unique<TaskQueue>(*c.repos);
  c.scheduler = std::make_unique<Scheduler>(*c.queue);
  c.intents = std::make_unique<IntentClassifier>(*c.llm);
  c.lines = std::make_unique<LineGenerator>(*c.llm, c.personas, c.scripts);
  c.triage = std::make_unique<EmailTriage>(c);
  c.wake_ladder = std::make_unique<WakeLadder>(c);
  c.departure = std::make_unique<Departure>(c);
  c.conversation = std::make_unique<Conversation>(c);
  c.contacts = std::make_unique<ContactService>(c);
  c.commentator = std::make_unique<Commentator>(*c.reflex, *c.lines);
  c.trip_runner = std::make_unique<TripRunner>(c);
  c.load_sim = std::make_unique<LoadSimulator>();
  c.audio = std::make_unique<AudioCache>(c.env.root() + "/backend/audio_cache");

  // Call events from any voice provider go through the Conversation, and its reply is spoken back.
  c.voice->onCallEvent([&c](const std::string& call_id, const std::string& event, const std::string& text) {
    if (auto line = c.conversation->onEvent(call_id, event, text)) {
      c.voice->say(call_id, line->text, line->persona, line->urgency, "");
      if (line->end_call) c.voice->hangup(call_id, "completed");
    }
  });

  // Scheduler handlers, one line per owner (T04 queue + loop).
  c.wake_ladder->registerHandlers(*c.scheduler);  // WAKE_CALL (T10)
  c.departure->registerHandlers(*c.scheduler);    // DEPARTURE_CHECK (T10)

  LOG_INFO("providers", "voice=" << c.providers.voice << " llm=" << c.providers.llm << " speech=" << c.providers.speech
                                 << " reflex=" << c.providers.reflex << " calendar=" << c.providers.calendar
                                 << " email=" << c.providers.email << " router=" << c.providers.router
                                 << " transit=" << c.providers.transit);
  return ctx;
}

}  // namespace enro
