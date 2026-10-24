# Prototype learnings

A parallel TypeScript/Express prototype (port 3333, plain HTML console) was built on Oct 24 2026 while this repo's C++ line was being set up. It focused on one live path: real Twilio call + phone motion + Google event → wake/leave time. This page records what it established so the C++ tasks don't relearn it. Snapshot as of the prototype's handoff (4:10 PM PT).

- Full prototype, code and tests: branch **`prototype-handoff-2026-10-24`** (`TEAM_HANDOFF.md` there is the long version).
- Raw research and planning notes, verbatim: [`docs/research/`](research/README.md). Historical; they don't override `PRD.md` or `TASKS.md`.
- Runbooks adapted to main: [`PHONE_TEST.md`](PHONE_TEST.md), [`GOOGLE_SETUP.md`](GOOGLE_SETUP.md).

`PRD.md` and `TASKS.md` remain the authority. Where a learning conflicts with the PRD, it's flagged below for the task owner to decide, not silently applied.

## Product decisions

| Learning | Informs |
|---|---|
| "UTC" in the original example means **Westfield UTC** (University Towne Centre mall, San Diego), not the time standard. An 8:30 event there was illustrative only. | T22 demo script, seed data (S3) |
| An email's **receive time is not the event time**. Date, time, timezone, location, and attendance must be established separately. | T09 triage, T03 Gmail |
| **Motion is not wakefulness.** The detector only shows sustained phone acceleration. It can't tell walking from shaking, and it doesn't classify posture, steps, toothbrushing, or sleep. | T10, T11, S7, pitch |
| Wake/test success = spoken or keypad check-in **plus** fresh motion after answer. Departure confirmation was voice/keypad only. | T10, T11, S7 |
| In the prototype, snooze explained the consequence but **did not reschedule** or create a callback. Main's PRD does negotiate snoozes (7.11 GRANT/COUNTER/REFUSE). That part is new work, unproven by the prototype. | T11 |
| Calendar events and structured ICS invites supply event facts. Free-form email text needed manual confirmation. No LLM extraction existed. | T09 |
| The everyday interface is the call plus movement. A screen is still needed for permissions, connections, and judge evidence. Don't claim screen-free onboarding unless T17 shows it. | T17, T18, pitch |

## Safety and ambiguity rules

These were implemented and covered by isolated tests in the prototype. They're cheap to port and prevent the worst demo failure: calling someone twice or at the wrong time.

| Rule | Informs |
|---|---|
| **No automatic retry on an ambiguous call create.** A timeout after `POST /Calls` may still have placed the call. Check the carrier log or reconcile by Call SID before trying again. | T02, T10 |
| **Unknown after restart.** Calls or dispatches that were active when the process died become `unknown` and need reconciliation, not a retry. Don't clear state to get around this. | T04, T02 |
| **Missed jobs don't burst-fire.** A job more than **2 minutes** late is marked missed instead of firing. Restarting after downtime must not trigger a string of catch-up calls. PRD 7.1 says "a restart loses nothing". Persisting is right, but re-firing stale wake calls isn't. | T04 |
| **15 s cooldown** between call attempts. | T02, T10 |
| **Plans start disarmed.** Arming is an explicit action before real calls go to a real phone. A background refresh must never undo a user's disarm. Main's fake mode doesn't need this; real `VOICE_PROVIDER=twilio` does. | T10, T04 |
| Reserve work durably **before** dispatch, so a crash mid-dispatch shows up as unknown rather than not-sent. | T04 |
| Changed, missing, or unconfirmed source data **disarms or replans**. It never keeps the old plan armed. | T09, T03, T07 |
| Calls are bounded: 25 s ring timeout, 120 s max call, bounded `<Gather>` rounds. Recording disabled. | T02 |
| "Stop"/"cancel" or keypad **0** takes precedence over every success check. Negative speech can revoke an earlier "I'm up". | T11, T02 |
| Destination phone comes only from server config or consented contacts, never from a request body or email text. Incoming event text is data and can't authorize calls, new recipients, or credential changes. | T02, T10 (PRD 7.7) |
| Twilio webhooks require signature validation and a matching account and Call SID. OAuth uses a random, expiring, single-use `state`. | T02, T03 |

## Motion details (Sensor Logger → `POST /api/motion/<MOTION_INGEST_TOKEN>`)

Informs **S7**. Setup steps are in [`PHONE_TEST.md`](PHONE_TEST.md).

- Sensor: normal/calibrated **Accelerometer**, gravity removed, m/s² on both OSes. Ignore the uncalibrated sensor (g on iPhone). Don't rescale. Magnitude-based, so axis signs don't matter.
- HTTP Push batches every **1 s** with device ID, session ID, increasing message ID, and epoch-**nanosecond** timestamps.
- Freshness: a sample counts only if it's ≤ **5 s** old. Reject samples > **2 s** in the future. Phone must use automatic time.
- Moving = magnitude ≥ **1.2 m/s²** in at least **two distinct seconds spanning ≥ 1 s**. A single shake across a second boundary doesn't count. This threshold is **not physically validated**.
- Never merge across devices or recording sessions. Reject duplicate, replayed, or out-of-order messages.
- Retention: bounded to 120 s / 12,000 readings in memory. Keep only a per-call motion summary.
- During a call, only evidence timestamped **after answer** counts. A test call shouldn't start unless the stream is fresh.
- The ingest token is a scoped secret: rate-limit and size-limit the endpoint, and give the token no admin rights.
- Locked-phone and background collection is the biggest unknown (the collector may need background location permission). If it can't be shown, drop the movement claim and keep the call plus check-in.

## Validation status

**Established (prototype only):** 58 passing TypeScript tests (core, providers with injected fake HTTP, isolated real-HTTP server tests), typecheck, and build. Twilio account authenticated and one US local voice number purchased (about $1.15/month). Tunnel health 200 with unauthenticated access blocked. None of this was run against the C++ backend.

**NOT yet proven anywhere:**

- a real call placed by Enro and answered
- real motion arriving during an answered call
- streaming while the phone is locked
- Google OAuth sign-in and a real Calendar/Gmail import
- a live Routes calculation
- a scheduled (not manually triggered) call firing on time
- LLM conversation or natural-language email understanding (the prototype had none)
- overnight reliability, multi-user isolation

The prototype's runtime snapshot showed 0 motion samples, 0 calls, 0 events, and Google/Routes unconfigured. Its next steps map onto main as: S7 + T02 phone trial ([`PHONE_TEST.md`](PHONE_TEST.md)) → T03 Google import → T03 Routes → T04/T10 near-future scheduled call → T22 rehearsal.

## Hackathon context

- Event: **c0mpiled-16: UCSD** (Startup Incubator Club with Transpose Platform), Oct 24 2026. Theme **Invisible Interfaces**. Judging on scalability, innovation, real-world impact (no published weights). One track, fresh build, and **disclose AI coding-tool use**.
- The prototype's research (official Luma page plus the reproduced Hacker Guide) recorded a **6:30 PM PT submission deadline**, with a 5:45 PM feature-freeze target.
- **Unresolved discrepancy:** `PRD.md` section 16 plans a **24-hour** timeline. These schedules are not equivalent. Someone should confirm the real deadline with the organizers and update PRD 16 / `docs/DEMO.md`. This doc doesn't pick one.
- Track: the prototype research leaned Consumer AI, which matches PRD 2 (Track 4). No final track selection was recorded there.
- Research notes on past winners (`research/MATTHEW_KIM_RESEARCH.md`): a specific problem, a visible change, technology that actually enables it, and evidence judges can inspect. Describe awards precisely. Gist was top five, not first.

## Demo causal chain

The prototype's demo target, which fits PRD 14 beats 3–4:

**a commitment changes → timing changes → a real phone rings with the reason → fresh motion and the user's response change the call outcome.**

- Use one event and one person. Show the source, change its time or venue, show the revised wake/leave lines (T07 reasoning), ring the phone, walk and check in, end on timestamped evidence (T18 dashboard).
- Illustrative arithmetic only: 8:30 event, 25 min travel, 45 min prep, 10 min buffer → 7:10 wake / 7:55 leave. The 25 min was not a measured route.
- The prototype had no accelerated clock. Main has `/api/demo/trigger`. Say so when using it.
- A recording, the browser call simulator, or fixtures are fallbacks. Label them as such on stage.

## Explicit cuts (from the prototype's scope)

Toothbrushing/activity recognition, sleep-stage inference, custom native app, app-store launch, multiple users or households in the live path, broad inbox ingestion, general life-assistant behavior, unlimited nagging, production deployment and overnight hosting. The prototype also cut real snooze rescheduling. Main's PRD keeps snooze negotiation (T11), so that cut does not carry over automatically.

## Setup facts worth keeping

- Twilio voice-only needs no SMS registration. Main's T10 contact SMS does: US A2P 10DLC registration may be needed for SMS from a local number. Check early (PRD 17).
- Marketplace/plugin searches found no usable calling or calendar product. Use the direct APIs.
- Secrets and real numbers live only in an ignored `.env`. The prototype's account access can be shared privately, but nothing from it belongs in this repo.
