# Enro PRD

> Ontime everytime.

Motto for the landing page, README, pitch close, and every docs header. "It knows you'll be late before you do" stays as the one-line explainer under it.

## 1. Summary

Enro is a personal AI agent that runs in the background and gets you where you need to be. It wakes you up, gets you out the door, guides you there, and learns how late you really run. You talk to it by voice. After a one-time setup it has no interface.

It reads your calendar and email to find what's coming, decides how urgent each thing is, and reaches you by phone call or earbuds with a personality that adapts to you. If you don't wake up, it escalates and alerts the people you chose.

## 2. Hackathon context

**Challenge** Invisible Interfaces. Help people sense, decide, and act without a traditional app or screen.

**Primary track** Track 4, AI-Powered Consumer Products for 1 Billion People.

**Secondary angles** Track 3 (Unsupervised). The agent runs all day and decides when to stop. Track 1 (Dispatch). Live transit GPS moves people where they need to go.

**Judging** Top 5 demo after scoring on scalability, innovation, and real world impact. Section 4 maps each criterion to a demo beat.

## 3. What makes it new

Leave-now alerts and AI wake-up calls already exist separately. Enro is different in four ways.

1. **Personal punctuality model.** It learns your bias per event type. You leave 12 minutes later than planned for class. You underestimate walks. Formal events take longer to get ready for. It corrects the plan before you make the mistake.
2. **It talks back with your own numbers.** "Give me five minutes" is checked against the Consequence Engine and your history. "Last three times you said five, it was eleven."
3. **It decides when to speak.** A fast reflex layer (Jev) filters a constant stream of calendar, email, transit, and location events. Most produce silence. The LLM only runs when Enro has something worth saying.
4. **Escalation reaches real people.** If you sleep through everything, your chosen emergency contacts hear about it.

## 4. Judging criteria map

### Scalability
- Server-side agent. Users need no device beyond a phone that takes calls.
- Scheduler is a min-heap priority queue keyed by next action time. Workers shard users by wake minute.
- LRU caches for parsed emails, routes, and TTS audio cut API calls and cost.
- Jev on the hot path (about 50 to 150 ms per decision, vendor numbers, measure your own). LLM only when speaking.
- GTFS-realtime is a standard feed used by transit agencies worldwide. One pipeline covers many cities.
- **Demo beat.** Load simulation plans 100,000 synthetic users and shows runtime, calls-per-minute histogram, and cost per user per month.

### Innovation
- Punctuality model, negotiation with real numbers, reflex layer deciding when to talk, live trip commentary.
- **Demo beat.** Show the learned biases and a live Jev decision feed on the dashboard.

### Real world impact
- Lateness costs people classes, shifts, interviews, and trust. Transit riders lose the most to delays and detours.
- Calls work on any phone. No smartphone app required.
- **Demo beat.** One slide with two or three cited stats found during the event. Do not invent numbers.

## 5. User flow

### Onboarding (one time, about 3 minutes)
1. User opens the website and taps Start. The agent speaks through the browser mic.
2. Voice interview. Name, phone number, home area, usual wake time and whether they set their own alarm, how long they take to get ready for casual vs formal events, preferred transport by trip type, how harsh they want it.
3. Emergency contacts by voice. User says each name and relationship. Agent reads it back with the number's last four digits and asks for confirmation. Agent warns that names must match what the user will say later.
4. Google sign-in with Calendar read-only and Gmail read-only scopes.
5. Agent picks a starting personality from the answers and says "I've got it from here." The page shows a single "You're set" state. No dashboard for the user.

### Daily use (background)
1. **Night before.** Pull tomorrow's events and recent plan-changing emails. Compute wake time and urgency. Schedule the backup alarm call.
2. **Morning.** The user's own alarm rings first. If no awake signal by the grace window, Enro calls. Escalation ladder in 7.6.
3. **Before each event.** Ready-to-Leave Engine schedules a reminder at the right lead time with the right tone.
4. **Departure.** Enro confirms you left (location movement or verbal confirmation). Keeps nudging until it does.
5. **In transit.** Earbud updates in commentator style. Reroutes during delays.
6. **After each event.** Logs arrival minus start. Updates the punctuality model, transport preferences, and personality.
7. **Anytime.** Plan-changing emails (canceled class, moved interview) update the plan and may move tomorrow's wake call.

## 6. Scope

### P0 must ship
- Voice onboarding in the browser that saves a user profile and emergency contacts.
- Google OAuth (Calendar and Gmail read-only) with polling and sync tokens.
- Email triage. Reflex filter, then LLM extraction of plan-changing events.
- Priority-queue scheduler driving all agent actions.
- Ready-to-Leave Engine and Consequence Engine.
- Outbound calls through Twilio with OpenAI-generated persona lines and TTS.
- Turn-based conversation with intent classification and snooze negotiation.
- Morning escalation ladder with repeat calls and emergency contact SMS.
- Punctuality model seeded with three weeks of fake history.
- Trip replay of one recorded bus ride with live commentary and one reroute.
- Judge dashboard with plan math, Jev decision feed, call log, transcript.
- Load simulation.

### P1 should ship
- Personality adaptation from replies and outcomes.
- Transport mode learning by destination type and distance.
- Live GTFS-realtime feed for one agency.
- Routes and ETAs from Google Maps for walk, bike, drive, transit.
- Departure verification through a location beacon page.
- Friend ETA sharing ("Want me to tell Alex you're running late?").

### P2 stretch
- Native companion app (permissions only) for sleep data, existing alarms on Android, contacts, background location.
- Streaming voice for calls (Twilio Media Streams to OpenAI Realtime).
- Earbud audio delivery through a native app.
- Multi-city transit.

### Out of scope
- Full computer access. Use scoped OAuth only.
- Payments, social leaderboards.

## 7. Core systems (backend owner implements)

### 7.1 Scheduler
- Min-heap of `AgentTask { due_at, user_id, kind, payload }`. Kinds are `PLAN_TOMORROW`, `WAKE_CHECK`, `WAKE_CALL`, `LEAVE_REMINDER`, `DEPARTURE_CHECK`, `TRANSIT_TICK`, `POLL_CALENDAR`, `POLL_GMAIL`, `POST_EVENT_LOG`.
- One loop pops due tasks and dispatches to handlers. Handlers may push new tasks.
- Tasks are persisted to SQLite so a restart loses nothing.
- Every action writes a `Decision` row for the dashboard.

### 7.2 Email triage
1. Poll Gmail with `historyId` for new messages.
2. Reflex filter on subject, sender, and snippet. Verdict is `IGNORE`, `MAYBE`, `PLAN_CHANGE`.
3. Only `PLAN_CHANGE` goes to the LLM for structured extraction (event, new time, canceled, location).
4. Store the extracted fact. Never store email bodies.
5. LRU cache keyed by message id so nothing is parsed twice.

### 7.3 Urgency score (0 to 100)
Inputs are event type (interview, exam, flight high; class, meeting medium; casual low), minutes of slack, number of events today, deadlines, and the user's lateness bias for that type. Levels are CHILL, FIRM, URGENT. Weights live in one config struct.

### 7.4 Ready-to-Leave Engine
```
prep_minutes   = base_prep[event_type] + learned_prep_bias[event_type]
travel_minutes = eta(mode_for(event), origin, destination) * learned_travel_factor[mode]
leave_by       = event.start - travel_minutes - buffer
reminder_at    = leave_by - prep_minutes
```
Every step writes a reasoning line.

### 7.5 Consequence Engine
Given the plan and a proposed delay, return each affected event with status OK, AT_RISK, or MISSED and minutes late. Used in negotiation, call openers, and the dashboard.

### 7.6 Morning escalation ladder
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

### 7.7 Emergency contacts
- Stored with name, relationship, phone, and confirmation status.
- Contact added in onboarding receives a one-time SMS saying they were added and can reply STOP.
- Only confirmed, non-revoked contacts are ever messaged. This check is enforced in code and tested.
- Messages identify themselves as automated messages from Enro. Tone is light. No private calendar details beyond "they have something at 9."

### 7.8 Punctuality model
Per user, per event type, and per mode, keep exponentially weighted averages.
- `prep_bias` (actual leave minus planned leave)
- `travel_factor` (actual travel over predicted travel)
- `arrival_delta` (arrival minus start)
- `channel_response` (answer rate by call vs earbud vs text, by time of day)
- `snooze_inflation` (actual snooze length over requested)

Seed three weeks of fake history for the demo user so the model has visible learned values.

### 7.9 Transport learning
Counts of mode by destination type and distance bucket. Default to onboarding answers. Predicted mode is the most frequent, with the stated preference as a tiebreaker.

### 7.10 Personality
Base archetypes in config (coach, best friend, drill sergeant, calm assistant). Live parameters are `warmth`, `harshness`, `humor`, `verbosity`, each 0 to 1. Replies and outcomes nudge them. Urgency level shifts tone per message. Persona never threatens anything except calling emergency contacts, and only when they exist.

### 7.11 Reflex layer (Jev)
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

### 7.12 Trip commentary
Replay a recorded trip (vehicle positions plus timestamps) at configurable speed. Each tick computes ETA and delay, asks the reflex layer whether to speak, and generates a commentator-style line when it should. One injected detour triggers a reroute suggestion.

## 8. Tech stack

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

### 8.1 Claude API alternate (commented out)

All text generation goes through an `LlmClient` interface with `chatJson(system, user, schema)` and `chat(system, messages)`. `OpenAIClient` implements it by default. `AnthropicClient` implements it too but stays fully commented out, ready to switch with `LLM_PROVIDER=anthropic`.

Scope of the switch
- Covers line generation, intent classification, email extraction, and onboarding profile extraction.
- Does not cover voice. TTS, transcription, and the Realtime onboarding session stay on OpenAI because the Claude API has no audio output.

What the commented `AnthropicClient` should contain
```cpp
// CLAUDE API ALTERNATE
// Switch with LLM_PROVIDER=anthropic and uncomment this file + its CMake entry.
// POST https://api.anthropic.com/v1/messages
// Headers: x-api-key: $ANTHROPIC_API_KEY, anthropic-version: <current>, content-type: application/json
// Body: { model: $ANTHROPIC_MODEL, max_tokens, system, messages: [{role, content}] }
// For chatJson, request JSON in the system prompt or use tool use with an input schema,
// then parse the first tool_use block's input. Response text lives in content[].text.
// Check current Anthropic docs for the version header, model names, and structured output options.
```

Why keep it
- Fallback if OpenAI rate limits or goes down during the event.
- Sponsor prize option if Anthropic is a sponsor.

## 9. Repo structure

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

## 10. Data models

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

## 11. API

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

## 12. Environment

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

## 13. Privacy and safety

- OAuth read-only scopes only. No computer access.
- Store extracted events, never email bodies.
- Refresh tokens stored server-side only.
- Emergency contacts are messaged only after consent. Enforced and tested.
- Driving mode is sparse. Only reroutes and arrival time.
- Google app runs in testing mode with listed test users. Production would need Google verification for Gmail scopes.

## 14. Demo script (3 minutes)

1. **Hook (15s)** "You set an alarm, it rings, you ignore it. Nobody knows you're about to be late. Enro does."
2. **Onboarding (25s)** Voice-only interview on the laptop. Confirms "Mom, ending in 4417." Google sign-in. "I've got it from here."
3. **Wake ladder (40s)** Interview day. Teammate ignores call 1 and 2. Call 3 yells. Mom's phone gets a text in parallel. Teammate answers and asks for five minutes. Enro counters with his history.
4. **Email drop (20s)** Send "Section canceled today" to the demo inbox. Dashboard shows the reflex verdict and the plan shifting.
5. **Trip replay (40s)** Recorded bus ride at 10x over the room speaker. Commentary. Injected detour. Reroute.
6. **Learning (15s)** Punctuality card. "You leave 12 minutes late for class. I already adjusted."
7. **Scale (20s)** Run 100,000-user simulation. Runtime, histogram, cost per user.
8. **Close (5s)** "Enro. Ontime everytime."

## 15. Skeleton and team split

The repo is a **skeleton**. Its job is to let several people work in parallel from hour one without blocking each other or touching the same files. Claude Code builds the frame, the contracts, and the mocks. The team builds everything else.

### 15.1 Skeleton rules
- **Contracts first.** Every module boundary is an interface (C++ abstract class or TypeScript type) plus example JSON in `contracts/`. Nobody changes a contract without telling the team.
- **Everything compiles and runs on day zero.** Every function exists and returns a fake but valid value marked `// STUB`. The whole app boots and the demo flow runs end to end on fake data.
- **One owner per folder.** Folders map to tasks in `TASKS.md`. Two people never edit the same file in the same task.
- **Mocks unblock frontend.** The frontend runs against a mock mode (`VITE_USE_MOCKS=true`) that reads fixtures from `contracts/examples/`. Frontend never waits on backend.
- **Fake providers unblock backend.** `VOICE_PROVIDER=sim`, `LLM_PROVIDER=fake`, `REFLEX_PROVIDER=rules`, `CALENDAR_SOURCE=seed`, `EMAIL_SOURCE=seed`. Backend logic can be built and tested with zero API keys.
- **Every stub has a TODO tag and a task ID.** Format `// TODO(T07): compute leave_by per PRD 7.4`. Running `make todos` lists open work by task.
- **Every module has a test file** with one passing placeholder test and TODO cases, so owners add tests without setting up anything.

### 15.2 Claude Code builds (skeleton only)
- Folder structure from section 9, plus `contracts/` and `TASKS.md`.
- CMake with FetchContent. `cmake -B build && cmake --build build` works on macOS and Linux. Protobuf for GTFS-RT uses `find_package` with install notes in the README and is optional at build time.
- Crow server boots. `/health` returns 200. Every route from section 11 is registered and returns fixture JSON from `contracts/examples/` with a `// STUB` marker.
- All headers with full declarations from section 10. Every `.cpp` has stubbed method bodies that return valid fake data.
- Interfaces with a fake implementation each. `LlmClient` (FakeLlm), `VoiceProvider` (SimProvider), `Reflex` (RulesReflex returning a fixed verdict), `CalendarSource` and `EmailSource` (Seed), `Router` (FallbackRouter with fixed speeds), `TransitFeed` (ReplayFeed).
- Real implementations stay as signatures with TODOs. `OpenAIClient`, `TwilioProvider`, `JevReflex`, `GoogleAuth`, `CalendarClient`, `GmailClient`, `GoogleRouter`, `GtfsRtFeed`.
- `AnthropicClient` and `RetellProvider` fully commented out per 8.1 and section 11.
- Infrastructure that every task depends on, built for real. `Env` loader, SQLite schema creation, dashboard websocket broadcasting events, provider selection from env, `Log` helper.
- Config and seed files with realistic content for one student demo user.
- Frontend shell. Router, all pages and components rendering with mock data, typed API client, websocket hook, mock mode, Vite proxy.
- `docs/` with headings and PRD content copied in. Makefile. README with setup and how to run in fake mode.
- `TASKS.md` from 15.3.

### 15.3 Task board (goes into TASKS.md)

Owners are placeholders. Fill in names at kickoff.

| ID | Task | Folder(s) | Owner | Depends on | Done when |
|---|---|---|---|---|---|
| T01 | OpenAI client (chat JSON, TTS, transcribe, realtime key) | `backend/src/ai/OpenAIClient*` | Backend B | none | Real calls work behind `LLM_PROVIDER=openai` |
| T02 | Twilio provider, TwiML, webhooks, signature check | `backend/src/voice/`, `http/TwilioRoutes.cpp` | Backend B | none | Phone rings with a TTS line |
| T03 | Google OAuth, Calendar and Gmail clients with sync tokens | `backend/src/google/`, `http/OAuthRoutes.cpp` | Backend B | none | Demo account events and emails load |
| T04 | Task queue loop and scheduler dispatch | `backend/src/scheduler/` | Mikey | none | Tasks fire on time from SQLite |
| T05 | LRU cache and wiring into triage, routing, audio | `backend/src/cache/` | Mikey | none | Tests pass, cache hit rate logged |
| T06 | Urgency score | `backend/src/planner/Urgency*` | Mikey | none | Matches 7.3 on seed data |
| T07 | Ready-to-Leave Engine | `backend/src/planner/ReadyToLeave*` | Mikey | T06 | Reasoning lines match 7.4 |
| T08 | Consequence Engine | `backend/src/planner/ConsequenceEngine*` | Mikey | T07 | Correct OK, AT_RISK, MISSED on seed |
| T09 | Email triage | `backend/src/triage/` | Mikey | T03 for live, seed otherwise | Canceled-class email shifts plan |
| T10 | Wake ladder and emergency contact notify | `backend/src/calls/WakeLadder*`, `contacts/` | Mikey | T02 for live, sim otherwise | Three calls then contact SMS |
| T11 | Conversation, intents, negotiation | `backend/src/calls/Conversation*`, `ai/IntentClassifier*` | Mikey | T08 | "Five more minutes" answered with numbers |
| T12 | Line generation and persona prompts | `backend/src/ai/LineGenerator*`, `config/personas.json` | Backend B | T01 | Tone changes by urgency |
| T13 | Punctuality model and transport learner | `backend/src/model/` | Mikey | none | Seed history yields visible biases |
| T14 | Jev reflex client | `backend/src/reflex/JevReflex*` | Backend B | none | Verdicts logged with latency, rules fallback on timeout |
| T15 | Trip replay and commentator | `backend/src/transit/` | Backend B | T12, T14 | Replay speaks and reroutes on detour |
| T16 | Load simulator | `backend/src/sim/LoadSimulator*` | Mikey | T07 | 100,000 users under 5 seconds |
| T17 | Voice onboarding page | `frontend/src/pages/Onboarding.tsx`, `hooks/useVoiceSession.ts` | Frontend | T01 realtime key | Profile saved from a spoken interview |
| T18 | Judge dashboard | `frontend/src/pages/Dashboard.tsx`, `components/` | Frontend | contracts only | Renders every websocket event type |
| T19 | Trip replay and load sim panels | `frontend/src/components/TripReplay.tsx`, `LoadSimPanel.tsx` | Frontend | contracts only | Driven by mock then live data |
| T20 | Landing, Done, Call simulator, Beacon pages | `frontend/src/pages/` (those files) | Frontend | contracts only | Pages work in mock mode |
| T21 | Visual identity, voice orb, persona voices | design files, `VoiceOrb.tsx`, `config/personas.json` voice fields | Design | none | Approved look and voice per persona |
| T22 | Pitch deck, impact stats, demo script rehearsal | `docs/DEMO.md`, slides | Design + all | T18 | Three full rehearsals |
| T23 | SCALE.md cost formula and sim results | `docs/SCALE.md` | Mikey | T16 | Numbers cited and on a slide |
| T24 | Record a real bus trip for replay | `backend/seed/trips/` | Anyone | none | One trip file with a detour segment |

### 15.4 Working agreement
- Branch per task, named `T07-ready-to-leave`. Small PRs. Merge to `main` often.
- `main` must always build and boot in fake mode.
- Changing anything in `contracts/` needs a heads-up in the team chat and an update to both the C++ struct and the TypeScript type in the same PR.
- Integration checkpoints at hours 6, 12, and 18. Everyone merges, the demo flow runs end to end, then work continues.

## 16. Timeline (24h)

| Hours | Goal |
|---|---|
| 0 to 2 | Skeleton builds. ngrok. Twilio number. Phone rings with hardcoded TwiML |
| 2 to 5 | TaskQueue loop, Ready-to-Leave, Consequence Engine with tests |
| 5 to 8 | Google OAuth, calendar poll, email triage on seed inbox |
| 8 to 11 | Wake ladder with real calls, contact SMS |
| 11 to 14 | Conversation, negotiation, persona lines |
| 14 to 16 | Voice onboarding end to end |
| 16 to 18 | Trip replay and commentator |
| 18 to 20 | Punctuality model on seeded history. Jev wired |
| 20 to 21 | Load sim. Dashboard polish. Feature freeze |
| 21 to 24 | Rehearse 5 times. Record backup video |

## 17. Risks

| Risk | Mitigation |
|---|---|
| Twilio trial only calls verified numbers | Verify all demo phones early |
| Google OAuth consent screen friction | Testing mode with team accounts added |
| Jev access or latency | Rules fallback behind the same interface |
| Phone network fails on stage | `VOICE_PROVIDER=sim` and recorded backup |
| Live transit feed down | Recorded trip replay |
| Judges ask "isn't this Google Maps" | Lead with punctuality model and negotiation |
| Privacy concerns | PRIVACY.md and one line in the pitch |

## 18. Success criteria

- Voice onboarding produces a working profile with confirmed contacts.
- Wake ladder escalates across three calls and texts the contact in parallel.
- A dropped email changes the plan live.
- Trip replay commentary reroutes on a detour.
- Punctuality card shows learned biases.
- Load sim runs live.
- Each judging criterion has a demo beat and a doc.
