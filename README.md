<p align="center"><a href="https://enro-sic.vercel.app"><img src="prototype/design/landing/enro logo.png" alt="enro" width="220"></a></p>

<h1 align="center">En route. On time.</h1>

<p align="center">A call to get you up. A little help getting out the door.</p>

<p align="center"><a href="https://enro-sic.vercel.app">Try Enro</a> · <a href="https://enro-sic.vercel.app/demo">Explore the demo room</a> · <a href="https://enro-sic.vercel.app/dashboard">Team dashboard</a> · <a href="docs/INTEGRATION.md">Build status</a></p>

[![Enro demo room](docs/assets/demo-preview.png)](https://enro-sic.vercel.app/demo)

Enro plans backward from your next commitment. It combines the event time, travel, time to get ready, and an arrival buffer. A phone call gives you a reason to get moving, with a reminder when it is time to leave.

Built for the SIC Invisible Interfaces challenge. The phone call is the daily interaction. The website handles setup and makes the reasoning visible during the demo.

## Try the demo

[The public site](https://enro-sic.vercel.app) works without an account. Choose a sample event, change your travel mode, preview a call, and see how a delay changes your departure. The demo room also includes the Crossroads to Price Center scenario, sensor evidence, and the Lookahead studio. The team dashboard uses its contract fixtures.

Public calls, events, sensor signals, and forecasts are illustrative. Your real inbox, phone number, location, and operator controls stay local.

## What is here

| Part | What it does | Status |
| :--- | :--- | :--- |
| Public demo | Sample morning, call preview, departure changes, and Lookahead | Runs on Vercel with sample data |
| Team build | C++ planner, durable scheduler, wake ladder, negotiation, learned punctuality, and React dashboard | Runs locally in fake mode; real providers are configured separately |
| Phone prototype | Twilio calls, Google event imports, route planning, and native phone sensor ingestion | Working local integration in `prototype/` |
| Walking check | Fresh counter increments, Activity, and spoken response | Experimental; revised rule still needs physical validation |

Both implementations are preserved in main. They have separate schedulers and call state. The public site brings their views together; it does not connect the two backends. [Integration notes](docs/INTEGRATION.md) define the handoff.

## Run locally

The team build needs a C++20 compiler, CMake 3.20 or later, Node 20 or later, and Python with `jsonschema` and `websocket-client`.

```sh
make dev-fake
```

Open [the team dashboard](http://localhost:5173/dashboard). Fake mode uses no API keys. The [team setup guide](docs/TEAM_BUILD.md) covers provider settings, commands, and architecture.

The phone prototype needs Node 22.13 or later.

```sh
cd prototype
npm ci
cp .env.example .env
npm start
```

Open [the local prototype](http://localhost:3333). Keep an existing `.env` when restarting. [Prototype setup](prototype/README.md), [Google setup](prototype/docs/GOOGLE_SETUP.md), and [phone test notes](prototype/docs/PHONE_TEST.md) explain the live integrations. Plans begin disarmed.

## What the phone test taught us

A real call and concurrent sensor delivery worked. A later trial exposed a false positive: seven vertical shakes were counted as seven steps, and the old counter plus voice rule ended the call. The user had not walked.

The revised experiment also requires fresh walking or running Activity. Phone position only describes the device. Neither a pedometer count nor a spoken response proves wakefulness. Walking versus shaking and background reliability still need physical tests. The public demo makes that distinction visible.

## Project map

| Path | Contents |
| :--- | :--- |
| `backend/` | C++ server, planning, scheduling, providers, and persistence |
| `frontend/` | React onboarding, dashboard, replay, and browser call simulator |
| `contracts/` | Shared schemas and fixtures |
| `prototype/` | TypeScript phone integration, tests, design originals, research, and demo exports |
| `prototype/public/demo/` | Interactive story, sample day, and Lookahead studio |
| `docs/` | Architecture, team setup, integration notes, pitch, privacy, and scale |
| `scripts/build_demo.mjs` | Builds the public site with fixtures and no operator pages |

## Check and publish

```sh
make test
make contracts
make e2e
npm --prefix frontend ci
npm --prefix frontend run build
npm --prefix prototype ci
npm --prefix prototype run typecheck
npm --prefix prototype test
node --test prototype/public/demo/stage-model.test.mjs prototype/public/demo/lookahead/model.test.mjs
node scripts/build_demo.mjs
```

Vercel uses `vercel.json` to build the public demo into `site-dist/`. No Twilio or Google credentials are needed for that deployment. See [the release notes](docs/RELEASE.md) for the verified checks.

The original [PRD](PRD.md), [task board](TASKS.md), and [architecture](docs/ARCHITECTURE.md) remain the team build references.

Needs CMake 3.20+, a C++20 compiler (Apple Clang 15+ or GCC 12+), Node 20+, and Python 3 with `pip install jsonschema websocket-client`.

```bash
make dev-fake      # builds the backend, runs it on :8080 with every provider faked, and Vite on :5173
```

Open http://localhost:5173/dashboard, then trigger the demo beats from the dashboard buttons or:

```bash
curl -X POST localhost:8080/api/demo/trigger -H 'Content-Type: application/json' -d '{"user_id":"u_demo","scenario":"wake_ladder"}'
# scenarios: wake_ladder, leave_reminder, email_drop
make sim           # 100,000-user load simulation
```

In fake mode the phone call happens in the browser at `/call/<call_id>` (the call simulator), lines come from `backend/config/scripts.json`, and calendar, email, and history come from `backend/seed/`.

Frontend alone, with no backend at all: `make frontend` (reads fixtures from `contracts/examples/`, `VITE_USE_MOCKS=true`).

## Make targets

| Target | What it does |
|---|---|
| `make build` | `cmake -S backend -B build && cmake --build build` |
| `make backend` | Run the backend with your `.env` |
| `make frontend` | Vite dev server on :5173, proxies `/api`, `/oauth`, `/audio`, `/ws` to :8080 |
| `make dev` | Backend with your `.env` plus frontend against it |
| `make dev-fake` | Same, with every provider forced to fake. The demo flow |
| `make test` | Backend doctest suite |
| `make contracts` | Every `contracts/examples/*.json` matches its schema |
| `make smoke` | Every section 11 route on a running backend matches its contract |
| `make e2e` | Headless golden path in fake mode on :18080 (boot, smoke, demo beats, dashboard feed) |
| `make sim` | 100,000-user load sim on a running backend |
| `make prerender` | Cache scripted TTS lines into `backend/audio_cache/` (needs `OPENAI_API_KEY`) |
| `make tunnel` | ngrok to :8080 for Twilio and OAuth callbacks |
| `make todos` | Open work, grouped by task ID |

Before a PR: `make test && make contracts && make e2e`.

## Configuration

Copy `.env.example` to `.env`. An empty `.env` (or none) means fake mode. Values in your shell override the file. Never commit `.env`.

### Flipping providers

Each task owner flips their own provider once it works. Everything else stays fake.

| Env var | Fake (default) | Real | Also needs |
|---|---|---|---|
| `VOICE_PROVIDER` | `sim` (browser call simulator) | `twilio` | `TWILIO_ACCOUNT_SID`, `TWILIO_AUTH_TOKEN`, `TWILIO_FROM_NUMBER`, `PUBLIC_BASE_URL` from `make tunnel` |
| `LLM_PROVIDER` | `fake` (canned lines) | `openai` or `anthropic` | `OPENAI_API_KEY` + `OPENAI_CHAT_MODEL`, or `ANTHROPIC_API_KEY` (+ optional `ANTHROPIC_MODEL`, default `claude-opus-5-5`) |
| `REFLEX_PROVIDER` | `rules` | `jev` | `TYPESAFE_API_KEY`, `JEV_MODEL` |
| `CALENDAR_SOURCE` | `seed` | `google` | `GOOGLE_CLIENT_ID`, `GOOGLE_CLIENT_SECRET`, then sign in at `/oauth/google/start` |
| `EMAIL_SOURCE` | `seed` | `gmail` | same Google client |
| Routing | fixed speeds per mode | Google Routes when the key is set | `GOOGLE_MAPS_API_KEY` |

Voice (TTS, transcription, Realtime onboarding) always uses OpenAI when real, because the Claude API has no audio output.

### Google OAuth client

Create a **Web application** OAuth client in Google Cloud Console:

- Authorized redirect URI: `http://localhost:8080/oauth/google/callback`, plus `https://<ngrok-domain>/oauth/google/callback` when tunnelling
- Authorized JavaScript origin: `http://localhost:5173`
- Scopes: `calendar.events.readonly` and `gmail.readonly` only
- Consent screen in Testing mode with each teammate added as a test user

### Twilio

Trial accounts only call verified numbers, so verify every demo phone early. Point the number's voice webhook at `$PUBLIC_BASE_URL/twilio/voice` and its messaging webhook at `$PUBLIC_BASE_URL/twilio/sms`.

## Prototype learnings

A parallel TypeScript prototype (branch `prototype-handoff-2026-10-24`) worked through real-phone calling, phone motion, and Google setup. Its lessons, ported to this backend's routes:

- [`docs/PROTOTYPE_LEARNINGS.md`](docs/PROTOTYPE_LEARNINGS.md): decisions, safety rules, motion details, and what's still unproven, mapped to tasks
- [`docs/PHONE_TEST.md`](docs/PHONE_TEST.md): physical-phone runbook (tunnel, Twilio webhooks, Sensor Logger, acceptance steps)
- [`docs/GOOGLE_SETUP.md`](docs/GOOGLE_SETUP.md): OAuth client, scopes, Routes key, travel-mode nuances
- [`docs/research/`](docs/research/): the prototype's historical research notes, verbatim

## Repo layout

| Path | What |
|---|---|
| `backend/` | C++20 server: Crow, cpr, nlohmann/json, SQLiteCpp, all via CMake FetchContent |
| `backend/config/`, `backend/seed/` | Personas, canned lines, event types; the demo user's calendar, inbox, history, and bus trip |
| `frontend/` | React + TypeScript + Vite + Tailwind. Landing, onboarding, judge dashboard, call simulator, beacon |
| `contracts/` | JSON schemas and examples for every route and websocket event. Frozen; see `contracts/README.md` |
| `docs/` | Vision, architecture, demo script, scale, privacy, risks |
| `scripts/` | Contract check, smoke test, e2e, TTS prerender, how to record a bus trip |

GTFS-realtime (live transit) needs protobuf, which is optional at build time: `brew install protobuf` or `apt install libprotobuf-dev protobuf-compiler`. Without it the backend uses trip replay only.

## Working agreement

One task = one branch (`T07-ready-to-leave`) = one owner. Edit only the files your task owns. Small PRs, merged often. `main` always builds and boots in fake mode. In Claude Code, ship with `/shipit`.
