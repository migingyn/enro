# Architecture

> Ontime everytime.

How the backend, frontend, and providers fit together.

Copied from `PRD.md` at kickoff. PRD.md is the source of truth. Edit here only to add what the PRD doesn't cover.

## Request flow at a glance

```
 Calendar / Gmail (seed or Google)      Location beacon      Trip replay / GTFS-RT
              |                               |                       |
              v                               v                       v
        EmailTriage ---> Planner (Urgency -> ReadyToLeave -> ConsequenceEngine)
              |                 |                     ^
              v                 v                     | biases
        Reflex (Jev / rules) decides: speak or stay silent    PunctualityModel, TransportLearner
              |
              v
        Scheduler (TaskQueue min-heap) ---> WakeLadder / Conversation ---> VoiceProvider (sim | twilio)
              |                                                       \--> ContactService (consent gate) -> SMS
              v
        Decision rows in SQLite  +  /ws/dashboard broadcast (judges watch this)
```

Every module boundary is an interface with a fake implementation, picked from env at boot (`backend/src/app/App.cpp`). Fake mode is the default.

### 7. Core systems (backend owner implements)

#### 7.1 Scheduler
- Min-heap of `AgentTask { due_at, user_id, kind, payload }`. Kinds are `PLAN_TOMORROW`, `WAKE_CHECK`, `WAKE_CALL`, `LEAVE_REMINDER`, `DEPARTURE_CHECK`, `TRANSIT_TICK`, `POLL_CALENDAR`, `POLL_GMAIL`, `POST_EVENT_LOG`.
- One loop pops due tasks and dispatches to handlers. Handlers may push new tasks.
- Tasks are persisted to SQLite so a restart loses nothing.
- Every action writes a `Decision` row for the dashboard.

#### 7.2 Email triage
1. Poll Gmail with `historyId` for new messages.
2. Reflex filter on subject, sender, and snippet. Verdict is `IGNORE`, `MAYBE`, `PLAN_CHANGE`.
3. Only `PLAN_CHANGE` goes to the LLM for structured extraction (event, new time, canceled, location).
4. Store the extracted fact. Never store email bodies.
5. LRU cache keyed by message id so nothing is parsed twice.

#### 7.3 Urgency score (0 to 100)
Inputs are event type (interview, exam, flight high; class, meeting medium; casual low), minutes of slack, number of events today, deadlines, and the user's lateness bias for that type. Levels are CHILL, FIRM, URGENT. Weights live in one config struct.

#### 7.4 Ready-to-Leave Engine
```
prep_minutes   = base_prep[event_type] + learned_prep_bias[event_type]
travel_minutes = eta(mode_for(event), origin, destination) * learned_travel_factor[mode]
leave_by       = event.start - travel_minutes - buffer
reminder_at    = leave_by - prep_minutes
```
Every step writes a reasoning line.

#### 7.5 Consequence Engine
Given the plan and a proposed delay, return each affected event with status OK, AT_RISK, or MISSED and minutes late. Used in negotiation, call openers, and the dashboard.

#### 7.6 Morning escalation ladder
```
user alarm time
  + grace (5 min) → no awake signal → CALL_1 (calm)
  + 3 min        → no answer      → CALL_2 (louder voice, harsher persona)
  + 3 min        → no answer      → CALL_3 (full yelling) AND notify emergency contacts in parallel
  + answer at any step            → awake check ("What's your first thing today?")
  pass awake check                → AWAKE, schedule a 5 min verification call
```
Notes
- Apps cannot raise system volume. Escalate through voice intensity, persona, and repeat calls.
- On iPhone, a second call from the same number within three minutes can break through Do Not Disturb when "Allow Repeated Calls" is on. Onboarding tells the user to check it.
- iOS does not expose Clock alarms to third-party apps. Onboarding asks the alarm time instead. Android can read the next alarm through a native app (P2).

#### 7.7 Emergency contacts
- Stored with name, relationship, phone, and confirmation status.
- Contact added in onboarding receives a one-time SMS saying they were added and can reply STOP.
- Only confirmed, non-revoked contacts are ever messaged. This check is enforced in code and tested.
- Messages identify themselves as automated messages from Enro. Tone is light. No private calendar details beyond "they have something at 9."

#### 7.8 Punctuality model
Per user, per event type, and per mode, keep exponentially weighted averages.
- `prep_bias` (actual leave minus planned leave)
- `travel_factor` (actual travel over predicted travel)
- `arrival_delta` (arrival minus start)
- `channel_response` (answer rate by call vs earbud vs text, by time of day)
- `snooze_inflation` (actual snooze length over requested)

Seed three weeks of fake history for the demo user so the model has visible learned values.

#### 7.9 Transport learning
Counts of mode by destination type and distance bucket. Default to onboarding answers. Predicted mode is the most frequent, with the stated preference as a tiebreaker.

#### 7.10 Personality
Base archetypes in config (coach, best friend, drill sergeant, calm assistant). Live parameters are `warmth`, `harshness`, `humor`, `verbosity`, each 0 to 1. Replies and outcomes nudge them. Urgency level shifts tone per message. Persona never threatens anything except calling emergency contacts, and only when they exist.

#### 7.11 Reflex layer (Jev)
Typed decisions only. The LLM still writes every spoken line.

| Decision | Verdicts |
|---|---|
| Email relevance | IGNORE, MAYBE, PLAN_CHANGE |
| Speak on transit update | IGNORE, COMMENT, ALERT, REROUTE |
| Interrupt channel | WAIT, EARBUD, CALL |
| Readiness from reply | READY, STALLING, UNCLEAR |
| Snooze request | GRANT, COUNTER, REFUSE |
| Agent stop condition | CONTINUE, BACK_OFF, STOP, ESCALATE |

Every decision has a rules-based fallback used on timeout or missing key. Jev is called over HTTP (TypeSafe API, or OpenRouter as a second route). Verify the current API shape and model name before wiring it.

#### 7.12 Trip commentary
Replay a recorded trip (vehicle positions plus timestamps) at configurable speed. Each tick computes ETA and delay, asks the reflex layer whether to speak, and generates a commentator-style line when it should. One injected detour triggers a reroute suggestion.

### 8. Tech stack

| Layer | Choice |
|---|---|
| Backend | C++20, Crow (HTTP and websockets), cpr (HTTP client), nlohmann/json, SQLiteCpp, CMake with FetchContent |
| Tests | doctest or GoogleTest |
| Frontend | React, TypeScript, Vite, Tailwind, React Router |
| LLM | OpenAI chat with JSON output. Claude API alternate behind the same interface, commented out (see 8.1) |
| Voice out | OpenAI TTS with tone instructions |
| Voice in on calls | Twilio `<Gather input="speech">` |
| Voice onboarding | OpenAI Realtime over WebRTC in the browser with an ephemeral key minted by the backend. Fallback is push-to-talk with OpenAI transcription |
| Telephony | Twilio default. Retell alternate, fully commented out |
| Reflex | Jev over HTTP, rules fallback |
| Google | OAuth 2.0, Calendar API, Gmail API (REST through cpr, no C++ client library exists) |
| Routing | Google Maps Routes API, fallback to fixed speeds per mode |
| Transit | GTFS-realtime (protobuf) for live, JSON recording for replay |
| Tunnel | ngrok for Twilio and OAuth callbacks |

Model names and all keys come from env vars.

#### 8.1 Claude API alternate

All text generation goes through an `LlmClient` interface with `chatJson(system, user, schema)` and `chat(system, messages)`. `OpenAIClient` implements it by default. `AnthropicClient` (`backend/src/ai/AnthropicClient.*`) implements it too: set `LLM_PROVIDER=anthropic` and `ANTHROPIC_API_KEY`, and optionally `ANTHROPIC_MODEL` (default `claude-opus-5-5`).

Scope of the switch
- Covers line generation, intent classification, email extraction, and onboarding profile extraction.
- Does not cover voice. TTS, transcription, and the Realtime onboarding session stay on OpenAI because the Claude API has no audio output.

How `AnthropicClient` calls the API
- `POST /v1/messages` with `anthropic-version: 2026-10-21`, `effort: low` (every call is on a live phone call or plan update), and `fallbacks: "default"` so a refused request is re-run server-side.
- `chatJson` uses structured outputs (`output_config.format` with a JSON schema). Callers write plain JSON Schema; the client adds `additionalProperties: false` and drops unsupported constraints.
- Any HTTP error, refusal, or bad JSON returns an empty result, and callers fall back to their rules.

Why keep it
- Fallback if OpenAI rate limits or goes down during the event.
- Sponsor prize option if Anthropic is a sponsor.

### 9. Repo structure

```
enro/
├── README.md
├── PRD.md
├── .env.example
├── .gitignore
├── TASKS.md                 # task board from 15.3
├── Makefile                 # dev, dev-fake, backend, frontend, tunnel, test, sim, e2e, prerender, todos
├── contracts/
│   ├── README.md            # how to change a contract
│   ├── schemas/             # JSON schema per API response and websocket event
│   └── examples/            # one example JSON per schema, used by stubs and frontend mocks
├── docs/
│   ├── VISION.md
│   ├── ARCHITECTURE.md
│   ├── DEMO.md              # beat-by-beat with judging map
│   ├── SCALE.md             # sharding plan, cost-per-user formula, sim results
│   ├── PRIVACY.md           # scopes, what is stored, what is not
│   └── RISKS.md
├── scripts/
│   ├── e2e.sh               # headless golden path using sim voice provider
│   ├── prerender.sh         # cache scripted audio
│   └── record_trip.md       # how to record a real bus ride for replay
├── backend/
│   ├── CMakeLists.txt
│   ├── config/
│   │   ├── personas.json
│   │   ├── event_types.json # base prep minutes, urgency weight
│   │   ├── scripts.json     # canned lines for keyless fallback
│   │   └── onboarding.json  # interview questions and slot schema
│   ├── seed/
│   │   ├── demo_user.json
│   │   ├── demo_history.json   # three weeks of trips and wake-ups
│   │   ├── demo_calendar.json
│   │   ├── demo_emails.json
│   │   └── trips/bus_trip_01.json
│   ├── src/
│   │   ├── main.cpp
│   │   ├── config/Env.hpp/.cpp
│   │   ├── http/
│   │   │   ├── Routes.hpp/.cpp
│   │   │   ├── ApiRoutes.cpp
│   │   │   ├── OAuthRoutes.cpp
│   │   │   ├── OnboardingRoutes.cpp
│   │   │   ├── TwilioRoutes.cpp
│   │   │   ├── RetellRoutes.cpp        # fully commented out
│   │   │   └── DashboardSocket.hpp/.cpp
│   │   ├── models/                      # structs from section 10
│   │   ├── store/Database.hpp/.cpp, Repos.hpp/.cpp
│   │   ├── scheduler/
│   │   │   ├── TaskQueue.hpp/.cpp       # min-heap + persistence
│   │   │   └── Scheduler.hpp/.cpp       # loop and dispatch
│   │   ├── cache/LruCache.hpp           # generic template
│   │   ├── google/
│   │   │   ├── GoogleAuth.hpp/.cpp
│   │   │   ├── CalendarClient.hpp/.cpp
│   │   │   └── GmailClient.hpp/.cpp
│   │   ├── triage/EmailTriage.hpp/.cpp
│   │   ├── planner/
│   │   │   ├── Urgency.hpp/.cpp
│   │   │   ├── ReadyToLeave.hpp/.cpp
│   │   │   └── ConsequenceEngine.hpp/.cpp
│   │   ├── model/
│   │   │   ├── PunctualityModel.hpp/.cpp
│   │   │   ├── TransportLearner.hpp/.cpp
│   │   │   └── PersonalityState.hpp/.cpp
│   │   ├── reflex/
│   │   │   ├── Reflex.hpp               # interface, one method per decision
│   │   │   ├── JevReflex.hpp/.cpp
│   │   │   └── RulesReflex.hpp/.cpp     # fallback
│   │   ├── ai/
│   │   │   ├── LlmClient.hpp            # interface for text generation
│   │   │   ├── OpenAIClient.hpp/.cpp    # chatJson, tts, transcribe, realtimeSession
│   │   │   ├── AnthropicClient.hpp/.cpp # fully commented out, Claude API alternate
│   │   │   ├── IntentClassifier.hpp/.cpp
│   │   │   └── LineGenerator.hpp/.cpp
│   │   ├── routing/
│   │   │   ├── Router.hpp
│   │   │   ├── GoogleRouter.cpp
│   │   │   └── FallbackRouter.cpp
│   │   ├── transit/
│   │   │   ├── TransitFeed.hpp
│   │   │   ├── ReplayFeed.cpp
│   │   │   ├── GtfsRtFeed.cpp           # P1
│   │   │   └── Commentator.hpp/.cpp
│   │   ├── voice/
│   │   │   ├── VoiceProvider.hpp
│   │   │   ├── TwilioProvider.hpp/.cpp
│   │   │   ├── TwiML.hpp/.cpp
│   │   │   ├── SimProvider.hpp/.cpp     # routes calls to browser
│   │   │   └── RetellProvider.hpp/.cpp  # fully commented out
│   │   ├── calls/
│   │   │   ├── WakeLadder.hpp/.cpp
│   │   │   └── Conversation.hpp/.cpp
│   │   ├── contacts/ContactService.hpp/.cpp   # consent gate
│   │   ├── sim/LoadSimulator.hpp/.cpp
│   │   ├── audio/AudioCache.hpp/.cpp
│   │   └── util/Log.hpp, Time.hpp/.cpp, Ewma.hpp
│   └── tests/
│       ├── test_task_queue.cpp
│       ├── test_lru_cache.cpp
│       ├── test_ready_to_leave.cpp
│       ├── test_consequences.cpp
│       ├── test_punctuality.cpp
│       └── test_contact_consent.cpp
└── frontend/
    ├── package.json, vite.config.ts, tailwind.config.ts, tsconfig.json, index.html
    └── src/
        ├── main.tsx, App.tsx
        ├── api/client.ts, api/types.ts
        ├── hooks/useDashboardSocket.ts, useVoiceSession.ts
        ├── pages/
        │   ├── Landing.tsx          # pitch line and Start
        │   ├── Onboarding.tsx       # voice-only, animated orb, live captions
        │   ├── Done.tsx             # "You're set"
        │   ├── Dashboard.tsx        # judges only
        │   ├── CallSimulator.tsx    # backup if phones fail
        │   └── Beacon.tsx           # phone location sender for departure verification
        └── components/
            ├── VoiceOrb.tsx
            ├── PlanTimeline.tsx
            ├── ReasoningLines.tsx
            ├── ReflexFeed.tsx
            ├── PunctualityCard.tsx
            ├── WakeLadder.tsx
            ├── TripReplay.tsx
            ├── Transcript.tsx
            └── LoadSimPanel.tsx
```

### 10. Data models

```cpp
struct User {
  std::string id, name, phone, timezone, home_area, archetype_id;
  std::string alarm_time;                 // user's own alarm, from onboarding
  int grace_minutes = 5;
  std::map<std::string, std::string> preferred_mode; // trip type -> mode
  double warmth, harshness, humor, verbosity;
};

enum class ConsentStatus { PENDING, CONFIRMED, REVOKED };
struct EmergencyContact {
  std::string id, user_id, spoken_name, relationship, phone;
  ConsentStatus consent; long long added_ts;
};

struct Event {
  std::string id, user_id, title, location, event_type, source; // source: calendar or email
  long long start_ts, end_ts; bool canceled = false;
};

enum class Urgency { CHILL, FIRM, URGENT };
struct LeavePlan {
  std::string event_id; std::string mode;
  int prep_minutes, travel_minutes, buffer_minutes;
  long long reminder_at, leave_by; Urgency urgency;
  std::vector<std::string> reasoning;
};

enum class TaskKind { PLAN_TOMORROW, WAKE_CHECK, WAKE_CALL, LEAVE_REMINDER, DEPARTURE_CHECK,
                      TRANSIT_TICK, POLL_CALENDAR, POLL_GMAIL, POST_EVENT_LOG };
struct AgentTask { long long due_at; std::string user_id; TaskKind kind; nlohmann::json payload; };

struct Decision { std::string user_id, kind, verdict, source; double confidence; long long ts; nlohmann::json inputs; };
// source: jev or rules

struct Outcome { std::string user_id, event_id, event_type, mode;
                 long long planned_leave, actual_leave, arrival; };

struct PunctualityStats { double prep_bias, travel_factor, arrival_delta, snooze_inflation; int samples; };

struct Call { std::string id, user_id, provider_sid, purpose, state; int attempt, turns; };
struct Turn { std::string call_id, speaker, text, intent; long long ts; };
```

### 11. API

| Method | Path | Purpose |
|---|---|---|
| GET | `/health` | Liveness |
| POST | `/api/onboarding/session` | Mint ephemeral OpenAI Realtime key and return onboarding instructions |
| POST | `/api/onboarding/complete` | Save structured profile and contacts from the interview |
| POST | `/api/contacts/:id/confirm` | User confirms a contact read-back |
| GET | `/oauth/google/start`, `/oauth/google/callback` | Google sign-in |
| GET | `/api/users/:id/plan?date=` | Events, leave plans, urgency, reasoning |
| GET | `/api/users/:id/consequences?delay=` | Consequence report |
| GET | `/api/users/:id/punctuality` | Learned stats |
| POST | `/api/location` | Beacon page location updates |
| POST | `/api/demo/trigger` | `{ user_id, scenario }` wake ladder, leave reminder, email drop |
| POST | `/api/demo/trip` | Start trip replay `{ trip, speed }` |
| POST | `/api/sim/load` | `{ users, threads }` |
| WS | `/ws/dashboard` | Pushes plan, decision, call_state, turn, trip_tick, sim_result |
| WS | `/ws/sim-call/:call_id` | Browser call simulator |
| POST | `/twilio/voice`, `/twilio/gather`, `/twilio/status`, `/twilio/sms` | Twilio webhooks |
| GET | `/audio/:id.mp3` | Cached TTS |

Validate `X-Twilio-Signature` on Twilio routes.

```cpp
// RETELL ALTERNATE (keep commented)
// Switch with VOICE_PROVIDER=retell. Retell owns STT, TTS, and turn-taking.
// Outbound via Retell create-phone-call with dynamic variables (persona, urgency,
// plan, consequence summary). Custom LLM websocket at /retell/llm/:call_id runs
// IntentClassifier + LineGenerator. Webhook at /retell/webhook.
// Check current Retell docs before enabling.
```

### 12. Environment

```
PORT=8080
PUBLIC_BASE_URL=
DB_PATH=./enro.db
DEMO_MODE=true
VOICE_PROVIDER=sim             # sim | twilio | retell

LLM_PROVIDER=fake              # fake | openai | anthropic (voice always uses OpenAI)
CALENDAR_SOURCE=seed           # seed | google
EMAIL_SOURCE=seed              # seed | gmail

OPENAI_API_KEY=
OPENAI_CHAT_MODEL=
OPENAI_TTS_MODEL=
OPENAI_REALTIME_MODEL=
OPENAI_TRANSCRIBE_MODEL=

TWILIO_ACCOUNT_SID=
TWILIO_AUTH_TOKEN=
TWILIO_FROM_NUMBER=

# RETELL_API_KEY=
# RETELL_AGENT_ID=

# ANTHROPIC_API_KEY=
# ANTHROPIC_MODEL=

REFLEX_PROVIDER=rules          # rules | jev
TYPESAFE_API_KEY=
JEV_MODEL=
# OPENROUTER_API_KEY=          # alternate route to Jev

GOOGLE_CLIENT_ID=
GOOGLE_CLIENT_SECRET=
GOOGLE_MAPS_API_KEY=

GTFS_RT_VEHICLE_URL=
GTFS_RT_TRIP_UPDATES_URL=
POLL_SECONDS=60
```

Defaults are the fake providers so a fresh clone runs with no keys. Each owner flips their own provider when their task is ready.

Frontend env
```
VITE_USE_MOCKS=true            # read fixtures from contracts/examples
VITE_API_BASE=http://localhost:8080
```
