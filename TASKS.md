# Enro task board

> Ontime everytime.

One row = one branch = one owner (or one cloud session). Folders map to tasks, so two people never edit the same file for different tasks. Fill in owner names at kickoff. Spec for every task is in `PRD.md`.

## Shared commands (the interface between tasks)

Every task can rely on these once S2 to S5 are merged:

| What | Command |
|---|---|
| Build backend | `cmake -S backend -B build && cmake --build build -j` |
| Run backend (fake mode) | `./build/enro` (reads `.env` at repo root; empty `.env` = all fake providers) |
| Backend tests | `ctest --test-dir build --output-on-failure` |
| Frontend | `npm --prefix frontend install && npm --prefix frontend run dev` (port 5173, proxies to :8080) |
| Contract check | `make contracts` |
| Everything | `make dev-fake`, `make test`, `make todos` |

## Phase 0: skeleton (do first, these four run in parallel)

Contracts (S1) are merged. S2 to S5 touch disjoint folders, so they can be four parallel cloud sessions. Every task below depends on S2 (backend) or S4 (frontend).

| ID | Task | Owns | Depends on | Done when |
|---|---|---|---|---|
| S1 | Contracts, models, types | `contracts/`, `backend/src/models/`, `frontend/src/api/types.ts` | none | Merged (PR #1) |
| S2 | Backend frame per PRD 15.2 | `backend/CMakeLists.txt`, `backend/src/**` (except `models/`), `backend/tests/**` | S1 | Builds on macOS + Linux, `/health` 200, every section 11 route returns its fixture, tests pass |
| S3 | Config and seed data | `backend/config/**`, `backend/seed/**` | S1 | Demo user, 3 weeks of history, calendar, emails, one bus trip with a detour |
| S4 | Frontend shell | `frontend/**` (except `src/api/types.ts`) | S1 | Every page and component renders with `VITE_USE_MOCKS=true` |
| S5 | Team tooling and docs | `Makefile`, `README.md`, `.env.example`, `docs/**`, `scripts/*.sh`, `scripts/record_trip.md` | S1 | `make todos` groups TODOs by task, README covers fake mode and how to flip providers |

## Phase 1: product (PRD 15.3)

**Cloud?** says whether a cloud session can finish the task without your machine. "Fake" means it's fully testable with zero keys. "Local" means it needs real keys, a phone, a mic, or a real bus ride.

| ID | Task | Folder(s) | Owner | Depends on | Cloud? | Done when |
|---|---|---|---|---|---|---|
| T01 | OpenAI client (chat JSON, TTS, transcribe, realtime key) | `backend/src/ai/OpenAIClient*` | Backend B | none | Local (key) | Real calls work behind `LLM_PROVIDER=openai` |
| T02 | Twilio provider, TwiML, webhooks, signature check | `backend/src/voice/`, `http/TwilioRoutes.cpp` | Backend B | none | Local (phone, ngrok) | Phone rings with a TTS line |
| T03 | Google OAuth, Calendar and Gmail clients with sync tokens | `backend/src/google/`, `http/OAuthRoutes.cpp` | Backend B | none | Local (OAuth) | Demo account events and emails load |
| T04 | Task queue loop and scheduler dispatch | `backend/src/scheduler/` | Mikey | none | Fake | Tasks fire on time from SQLite |
| T05 | LRU cache and wiring into triage, routing, audio | `backend/src/cache/` | Mikey | none | Fake | Tests pass, cache hit rate logged |
| T06 | Urgency score | `backend/src/planner/Urgency*` | Mikey | none | Fake | Matches 7.3 on seed data |
| T07 | Ready-to-Leave Engine | `backend/src/planner/ReadyToLeave*` | Mikey | T06 | Fake | Reasoning lines match 7.4 |
| T08 | Consequence Engine | `backend/src/planner/ConsequenceEngine*` | Mikey | T07 | Fake | Correct OK, AT_RISK, MISSED on seed |
| T09 | Email triage | `backend/src/triage/` | Mikey | T03 for live, seed otherwise | Fake | Canceled-class email shifts plan |
| T10 | Wake ladder and emergency contact notify | `backend/src/calls/WakeLadder*`, `contacts/` | Mikey | T02 for live, sim otherwise | Fake | Three calls then contact SMS |
| T11 | Conversation, intents, negotiation | `backend/src/calls/Conversation*`, `ai/IntentClassifier*` | Mikey | T08 | Fake | "Five more minutes" answered with numbers |
| T12 | Line generation and persona prompts | `backend/src/ai/LineGenerator*`, `config/personas.json` (prompt fields) | Backend B | T01 | Fake first, then local | Tone changes by urgency |
| T13 | Punctuality model and transport learner | `backend/src/model/` | Mikey | none | Fake | Seed history yields visible biases |
| T14 | Jev reflex client | `backend/src/reflex/JevReflex*` | Backend B | none | Local (key) | Verdicts logged with latency, rules fallback on timeout |
| T15 | Trip replay and commentator | `backend/src/transit/` | Backend B | T12, T14 | Fake | Replay speaks and reroutes on detour |
| T16 | Load simulator | `backend/src/sim/LoadSimulator*` | Mikey | T07 | Fake | 100,000 users under 5 seconds |
| T17 | Voice onboarding page | `frontend/src/pages/Onboarding.tsx`, `hooks/useVoiceSession.ts` | Frontend | T01 realtime key | Fake first, then local (mic) | Profile saved from a spoken interview |
| T18 | Judge dashboard | `frontend/src/pages/Dashboard.tsx`, `components/` (except the T19 and T21 files) | Frontend | contracts only | Fake | Renders every websocket event type |
| T19 | Trip replay and load sim panels | `frontend/src/components/TripReplay.tsx`, `LoadSimPanel.tsx` | Frontend | contracts only | Fake | Driven by mock then live data |
| T20 | Landing, Done, Call simulator, Beacon pages | `frontend/src/pages/` (those files) | Frontend | contracts only | Fake | Pages work in mock mode |
| T21 | Visual identity, voice orb, persona voices | design files, `VoiceOrb.tsx`, `config/personas.json` (voice fields) | Design | none | Local | Approved look and voice per persona |
| T22 | Pitch deck, impact stats, demo script rehearsal | `docs/DEMO.md`, slides | Design + all | T18 | Local | Three full rehearsals |
| T23 | SCALE.md cost formula and sim results | `docs/SCALE.md` | Mikey | T16 | Fake | Numbers cited and on a slide |
| T24 | Record a real bus trip for replay | `backend/seed/trips/` | Anyone | none | Local (bus ride) | One trip file with a detour segment |

**Best first wave of cloud sessions after Phase 0:** T04, T06, T13, T18, T20. They have no dependencies, run fully in fake mode, and touch separate folders. Then T07 → T08 → T11 and T16 as their dependencies land.

### Shared files (single owner, change only at checkpoints)

These files wire everything together. To avoid conflicts, tasks do **not** edit them. If you need a change, ask the owner or make it at an integration checkpoint.

| File | Owner |
|---|---|
| `backend/src/main.cpp`, `backend/src/app/**` (provider selection), `backend/src/http/Routes.*`, `backend/src/http/ApiRoutes.cpp`, `backend/src/http/DashboardSocket.*`, `backend/CMakeLists.txt`, `.github/workflows/**` | Mikey |
| `backend/src/config/**`, `backend/src/store/**`, `backend/src/util/**` (env, fixtures, SQLite, repos, seeding, log/time) | Mikey (add a Repos method when you need a query) |
| `.env.example`, `scripts/check_contracts.py`, `scripts/smoke_api.py` | Mikey |
| `frontend/src/App.tsx`, `frontend/src/api/client.ts`, `frontend/package.json` | Frontend |
| `contracts/**`, `backend/src/models/**`, `frontend/src/api/types.ts` | Whole team (see below) |
| `backend/config/personas.json` | T12 owns prompt fields, T21 owns voice fields |

### Files the PRD tree doesn't list (added in S2)

| File | Owner | Why |
|---|---|---|
| `http/OnboardingRoutes.cpp`, `ai/FakeLlm.*`, `ai/SpeechClient.hpp` | T01 | Realtime key and onboarding profile extraction are OpenAI work |
| `http/SimCallSocket.cpp`, `voice/SimProvider.*` | T02 | Part of the voice provider layer |
| `google/SeedSources.*`, `routing/GoogleRouter.cpp` | T03 | Google layer. Routes API is P1 and has no task of its own |
| `audio/AudioCache.*` | T05 | Cache wiring |
| `/api/location` departure check (stub in `ApiRoutes.cpp`) | T10 | P1 departure verification has no task of its own |
| `reflex/RulesReflex.*` | T14 | Rules are the Jev fallback |
| `transit/TripRunner.*` | T15 | Replay loop |
| `backend/tests/test_<module>.cpp` | The task named in the file's first line | One test file per module |

## Working agreement (PRD 15.4)

- Branch per task, named `T07-ready-to-leave`. Small PRs. Merge to `main` often.
- `main` must always build and boot in fake mode.
- Changing anything in `contracts/` needs a heads-up in the team chat and an update to both the C++ struct and the TypeScript type in the same PR.
- Integration checkpoints at hours 6, 12, and 18. Everyone merges, the demo flow runs end to end, then work continues.
- Ship with `/shipit` (Claude Code). It checks, commits, opens the PR, and auto-merges when it's clean.
