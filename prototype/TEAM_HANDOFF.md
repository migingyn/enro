# Enro — team context and build handoff

**Snapshot: October 24, 2026, 4:10 PM Pacific.** This is a dated handoff, not a live status page. The application and phone setup may advance after this commit. Check the running console before acting.

## Important: two implementations currently exist

This document's live status and test results describe the **local TypeScript/Express prototype**, published on **`prototype-handoff-2026-10-24`**. During publication, fetching the repository revealed that teammates had independently advanced `main` to **`5b04272`** with a **C++20 backend, shared API contracts, fake providers, and team tooling**. The two lines diverged after the original research snapshot. They have not been integrated.

The attempted direct push to `main` was rejected safely. This snapshot is being published separately so the running local prototype and teammates' work are both preserved. **Do not force-push, replace the team's `main`, or assume this branch is the agreed final architecture.**

| Workstream | This prototype snapshot | Team `main` as inspected at `5b04272` |
| --- | --- | --- |
| Backend | Node/TypeScript/Express in `src/`, port 3333 | C++20/Crow with SQLite and provider interfaces in `backend/`, documented port 8080 |
| Frontend | Plain HTML/CSS/JS in `public/` | React/TypeScript/Vite design and shared types in `frontend/`; shell work tracked separately |
| Default demonstration | Empty runtime; no synthetic live events or calls | Explicit fake/seed providers and browser call simulation; real providers are separate tasks |
| Commands | `npm start`, `npm test`, `npm run build` | `make dev-fake`, `make test`, `make contracts`, `make e2e` |
| Voice route | Per-call `/voice/:id` and status callback | Documented `/twilio/voice` and `/twilio/sms` contracts |
| Planning authority | Narrow live-call/sensor spike and this handoff | `PRD.md`, `TASKS.md`, frozen contracts and task owners |
| Validation established here | The recorded 58-test prototype suite and live readiness snapshot below | Not built, run, or independently validated in this handoff |

Read the team's [README](https://github.com/migingyn/enro/blob/5b04272/README.md), [task board](https://github.com/migingyn/enro/blob/5b04272/TASKS.md), [PRD](https://github.com/migingyn/enro/blob/5b04272/PRD.md), and [working rules](https://github.com/migingyn/enro/blob/5b04272/AGENTS.md) before changing that workstream. Its scope includes learned punctuality, a wake ladder, consented emergency-contact notification, voice onboarding, trip replay, load simulation, and additional model providers; those are **not features proven by this prototype**. Its planning text refers to a 24-hour event, whereas the earlier SIC research recorded the October 24 6:30 PM deadline. Reconcile that planning discrepancy with the team/organizer instead of silently treating both schedules as equivalent.

The team integration decision is outstanding: keep this branch as a physical-integration reference, port useful logic into the assigned C++ modules, or explicitly agree on a consolidated architecture. Do not wholesale merge the conflicting README, environment templates, or runtime assumptions. Twilio account access can be reused privately, but callback routes, configuration behavior, and safety checks need deliberate mapping.

For a separate, non-disruptive checkout of this prototype:

```sh
git clone --branch prototype-handoff-2026-10-24 https://github.com/migingyn/enro.git enro-prototype
```

## Read this first

Enro is a working local prototype for context-aware wake-up calls and departure reminders. The software paths exist, and Twilio is authenticated with a purchased voice number. **The defining real-phone demonstration has not passed yet.** At the snapshot, the running server reported:

| Item | Observed state |
| --- | --- |
| Calling configuration | Ready; account authentication and number ownership/voice capability were verified earlier through Twilio's API |
| Phone motion | Unavailable; **0 samples** |
| Call sessions | **0**; no Enro test call placed |
| Events / plans | **0 / 0** |
| Google OAuth configuration | Missing |
| Google account connection | Not connected |
| Google Routes configuration | Missing |

This snapshot came from a read-only request to the running local application's authenticated state endpoint. No test calls, sensor fixtures, event imports, or schedule mutations were performed to produce it.

**Immediate next step:** finish Sensor Logger setup on the physical phone and prove that fresh motion reaches Enro while an actual phone call is answered, including a locked-screen trial. Then connect Google and verify the event → route → wake/leave path.

## Product intent and decisions

The user chose an alarm that decides when to wake someone based on real commitments, calls them, and uses phone motion plus a spoken check-in to establish observable progress. The location idea is part of the same flow: determine when to get ready and leave, rather than treating departure as a separate product.

“UTC” in the original example means **Westfield UTC in San Diego**, not the UTC time standard. An example 8:30 event there is illustrative; it is not a currently imported commitment. An email received at 10:00 does not imply an event at 10:00. Event date, time, timezone, location, and attendance must be established separately.

The intended everyday interface is a phone call and movement. A screen is still useful for initial permissions, preferences, account connections, and operator/judge evidence. The current web UI is that setup and evidence console. It is not proof that all onboarding can happen without a screen.

The original brainstorm included detecting standing, walking, and toothbrushing. The implemented detector only establishes sustained **phone acceleration**. It does not classify steps, posture, toothbrushing, sleep stages, or wakefulness. Motion alone cannot distinguish a person walking from someone shaking the device.

The broader vision includes interpreting natural-language emails and conversational negotiation. The current implementation deliberately narrows this:

- Google Calendar events and structured ICS invitations supply event facts.
- Other email text requires manual event confirmation.
- Twilio speech recognition feeds bounded command handling; **there is no LLM integration yet**.
- Snooze explains the timing consequence but **does not change the schedule or create a callback**.
- Wake/test success requires a spoken or keypad check-in plus fresh movement. Departure confirmation currently needs voice/keypad confirmation only.

## Hackathon context and why this project

The event is **c0mpiled-16: UCSD**, with Startup Incubator Club and Transpose Platform, on October 24, 2026. The theme is **Invisible Interfaces**. The prior research recorded four directions: physical dispatch, screen-free interaction, autonomous workers that recognize limits, and ambient consumer AI.

The [official event page](https://luma.com/compiled-im3b) and the team's reproduced [Hacker Guide](https://docs.google.com/document/d/1h76F7lYYbtquW_43lZpmp_dveMFyaB3AZ0_zXf7E6KA/edit?tab=t.bot260rv2uqv) agree on a **6:30 PM Pacific submission deadline**. The guide lists **scalability, innovation, and real-world impact**, without numerical weights. It calls for one track, a fresh build, and disclosure of AI coding-tool use. Some start/finalist schedule details differed across sources; no exact pitch duration or required sponsor integration was established. These are findings from the earlier research, not newly reverified organizer announcements.

**Consumer AI is the working recommendation.** No App is plausible for the core voice/motion experience, but the published material did not clearly exempt setup screens. A final submission-track selection is not recorded. Do not present a recommendation as an official eligibility ruling.

Earlier briefs considered Apply Once, Known, Backfill, a café restocker, and other ideas. Those are historical alternatives, not parallel Enro deliverables. Kevin selected the wake/departure concept. Preserve the research because it explains tradeoffs, but follow this build's scope rather than an earlier ranking.

The Matthew Kim research suggested a repeatable pattern: a specific user problem, a visible useful change, technology that materially enables that change, and evidence judges can inspect. Awards must be described precisely: sponsor wins are not overall wins, his advertised total was not independently reconciled, and **Gist was described as top five, not first place**. Gist's useful lesson was prompt-to-workflow fit, source traceability, operating cost, and a deliberately scripted demonstration—not architecture size. See [the full research](.hackathon/MATTHEW_KIM_RESEARCH.md) for project-by-project sources and limitations.

For Enro, the visible causal chain should be: **a commitment changes → timing changes → a real phone rings with the reason → fresh motion and the user's response change the call outcome**. This is the demo target, not an observed completed run.

## What is built

| Area | Implementation | What remains unverified |
| --- | --- | --- |
| Console | Setup, live motion freshness, call evidence, event creation, route calculation, plan arming, audit history; desktop/mobile layout checks recorded | Physical-phone setup usability and full live demo |
| Motion | Native Sensor Logger HTTP ingestion, timestamp validation, replay rejection, per-device/session separation, movement heuristic | Real device delivery, threshold suitability, background/locked-phone behavior during a call |
| Voice | Real Twilio Calls API, speech/DTMF prompts, signed callbacks, stop, time bounds, carrier status/reconciliation | Ringing, answering, recognition, callback delivery, and live motion-aware completion on the actual phone |
| Calendar | Read-only OAuth, primary-calendar import, recurring instance expansion, filtering, source timezone | Live account authorization and import |
| Email | Bounded Gmail read path and structured ICS parser; attendance confirmation; manual review for other text | Live inbox import and real invitation-update handling |
| Travel | Google Routes requests for driving/walking/transit; traffic refinement and transit departure handling | Enabled/billed provider access and a real route |
| Scheduling | Durable wake/leave jobs, explicit arming, refresh/replanning, missed-job and uncertain-outcome handling | Live scheduled calls after real event/route connection |

Twilio setup is complete enough to attempt a call: the user completed its compliance profile and authorized **one US local voice number at $1.15/month plus taxes**. The Console showed an immediate $1.15 charge and the same recurring monthly number fee, with usage separate. This approval does not establish a budget for unrelated services or upgrades. No SMS registration is needed for the selected voice-only test workflow. Optional caller branding/registration flows were not completed.

Credentials and both phone numbers are intentionally omitted from this document. They are present only in ignored local configuration. A teammate cloning the repository will not inherit the account connection.

## Architecture and repository map

```text
Google Calendar / Gmail → normalized confirmed event
                              ↓
Google Routes + prep/buffer → wake time and leave time
                              ↓
durable local scheduler → Twilio outbound call → phone
                              ↑                    │
                         voice check-in            │
                              ↑                    │
Sensor Logger on phone → HTTPS tunnel → motion evidence
                              ↓
                    local operator console + audit
```

Runtime: Node.js **22.13+**, TypeScript, Express, `tsx`, Zod, Twilio SDK, `ical.js`, and native fetch for Google APIs. The frontend is plain HTML/CSS/JavaScript. `package-lock.json` pins the installed dependency graph. The repository retains its existing MIT license.

| Path | Purpose |
| --- | --- |
| `src/server.ts` | Routes, access checks, OAuth callback, event synchronization, call orchestration, scheduling and refresh loops |
| `src/types.ts` | Event, plan, route, motion, call, job, and audit contracts |
| `src/config.ts` | Environment loading and validated local credential updates |
| `src/store.ts` | Private JSON storage, access keys, audit persistence, restart uncertainty |
| `src/core/motion.ts` | Sensor payload validation and movement evidence |
| `src/core/planning.ts` | Plan arithmetic and due/missed-job selection |
| `src/providers/voice.ts` | Twilio requests, signatures, prompts, bounded command handling |
| `src/providers/google.ts` | OAuth, token refresh, Calendar/Gmail reads |
| `src/providers/invitations.ts` | ICS normalization and revision/cancellation handling |
| `src/providers/routes.ts` | Live routing requests and mode-specific timing |
| `public/` | Operator console |
| `test/` | Core/provider/voice tests and isolated real-HTTP server tests |
| `docs/PHONE_TEST.md` | Physical-phone setup and acceptance runbook |
| `docs/GOOGLE_SETUP.md` | Google access requirements and travel assumptions |
| `.hackathon/` | Research, concept history, build plan, tasks, service readiness |

The runtime stores events, plans, call metadata, job reservations, and audit history in private local JSON. Motion readings are kept in a bounded in-memory window; per-call motion summaries are preserved. This is a **single-user, single-process prototype**, not a replicated scheduler or production database deployment.

## Motion and phone behavior: important details

A telephone call does not expose the recipient's accelerometer. **Twilio and Sensor Logger are separate channels** joined by the Enro server.

Use the normal/calibrated **Accelerometer**, with gravity removed. Its `accelerometer` values are m/s² on both iOS and Android. Raw/uncalibrated iPhone acceleration can use g; Enro ignores that sensor. Do not multiply the accepted iPhone values by 9.80665. Axis sign conventions do not affect the magnitude-based detector. See the collector's [units](https://github.com/tszheichoi/awesome-sensor-logger/blob/main/UNITS.md) and [cross-platform reference](https://github.com/tszheichoi/awesome-sensor-logger/blob/main/CROSSPLATFORM.md).

- HTTP Push carries device/session identifiers, increasing message IDs, and epoch-nanosecond sample timestamps. Keep the default one-second batch interval.
- A sample must be fresh within **5 seconds**; samples more than **2 seconds in the future** are rejected. Automatic phone time is useful.
- The initial threshold is acceleration magnitude **1.2 m/s²**. Moving evidence requires at least two distinct moving seconds spanning at least one second; a brief shake crossing a second boundary is insufficient. This threshold is not physically validated yet.
- Different devices or recording sessions cannot combine into one movement result. Duplicate/replayed/out-of-order samples are rejected. Retention is bounded to 120 seconds / 12,000 accepted readings.
- A test call cannot start through the normal test endpoint until the motion stream is fresh. During a call, qualifying evidence must come after the answered timestamp.
- “I am up” or keypad **1** acknowledges the check-in. Negative speech can revoke a previous affirmative response. **Stop/cancel or keypad 0 takes precedence** over success checks.
- The ringing timeout is 25 seconds; the carrier call is limited to 120 seconds. The prompt loop also has round/time bounds. Unknown carrier outcomes are not automatically retried.
- Movement is evaluated as the voice interaction advances. The implementation does not classify a bathroom activity or automatically hang up just because a motion sample arrives.
- Collector background behavior needs testing on the exact device. Its documentation describes background location permission for continued collection; Enro does not use location samples for this detector. Microphone/camera collection should remain off.

The most recent setup interaction before this handoff was opening Enro's Connections panel, copying the private sensor URL, and launching iPhone Mirroring. Mirroring showed **connecting to an iPhone 15 Pro**. That does not prove Sensor Logger configuration or a connected phone stream. The subsequent runtime snapshot still showed zero samples.

Sensor Logger supports an app-generated `sensorlogger://config/{code}` export, but its public docs do not publish the code encoding for synthesizing an import ourselves. First-phone setup remains manual or through an available phone UI; export a reusable configuration only after it works. Do not invent a deep link or send its private token through an online QR generator.

## Event, route, and scheduler nuances

The calculation is deterministic:

```text
arrival deadline = event start − arrival buffer
leave time       = arrival deadline − route duration
wake time        = leave time − preparation duration
```

Defaults in the console are 45 minutes to prepare and a 10-minute arrival buffer. They are editable demonstration preferences, not learned estimates. Manual dates use the browser's local timezone; source events preserve their timezone. Plans start **disarmed**. Arming is an explicit action that enables real wake and departure calls to the configured test phone.

Calendar imports primary-calendar events for the next seven days and expands recurring instances. All-day, past, cancelled, declined, and unlocated events are excluded; tentative events are not treated as confirmed commitments.

Email import is bounded to ten recent matching candidates. One-off ICS invitations need a title, location, and unambiguous UTC/IANA start. Attendance must be confirmed before planning. Stable invitation identity/revisions preserve the attendance decision; cancellation invalidates the plan. Floating/all-day/recurring/ambiguous-DST or incomplete invitations stay in review. A message missing from a subsequent bounded import conservatively disarms its plan, but **absence is not proof of cancellation**. No natural-language email extraction or receipt-time shortcut is implemented.

Routing has no invented fallback duration:

- **Driving:** up to three traffic-aware requests refine departure timing; keep the longest observed duration. Google driving requests do not solve arrival time directly. This is a conservative approximation, not guaranteed on-time arrival.
- **Transit:** use the first scheduled transit departure minus preceding walking time. Merely subtracting a nominal trip duration can miss an infrequent bus/train.
- **Walking:** use the returned duration without driving traffic preferences.
- Routes must be fresh enough for planning; the core rejects estimates older than 15 minutes. Driving refinement can mean up to three billable API calls per estimate.

The scheduler checks every five seconds. Armed plans refresh source events and routes on five-minute loops. Changed/missing/unconfirmed source data disarms or replans; asynchronous refresh must not undo a user's disarm. Work is reserved durably before dispatch. Jobs over two minutes late are marked missed rather than causing a burst of catch-up calls. Call attempts have a 15-second cooldown.

A timeout after sending a call request is ambiguous: Twilio may have accepted it. Restarted active/dispatching work becomes **unknown**. Inspect carrier evidence and reconcile before any new attempt. Known carrier IDs have a reconciliation endpoint; an uncertain request without an ID still needs manual carrier-log investigation. Do not clear the state file to bypass this protection.

## Local operation and access

On the existing development machine, check `http://localhost:3333/health` and [the Connections panel](http://localhost:3333/#connections) before starting another process. The main build may already have a running server and tunnel. Do not restart them during an active call or phone test.

For a new checkout:

```sh
npm ci
# Only when .env does not already exist:
cp .env.example .env
npm start
```

In a separate terminal, run `npm run tunnel` using an installed `cloudflared`. Save the resulting HTTPS origin as `PUBLIC_BASE_URL` through **Connections → Configure service credentials**. The tunnel is temporary; both processes and the computer must stay running. A replacement tunnel requires updating the public base URL, the phone's push URL, and any Google OAuth callback registration that uses it. There is no deployed Vercel service or overnight reliability claim.

The private phone push URL is available through **Connections → Phone motion → Copy**. Paste it exactly into Sensor Logger's **Logger → settings → HTTP Push**, then start recording. Do not substitute `localhost` on the phone or append another path. The UI exposes separate calling readiness checks for credentials, caller number, destination, and public HTTPS URL.

| Configuration variable | Purpose / current setup |
| --- | --- |
| `PORT` | Local port; default 3333 |
| `PUBLIC_BASE_URL` | Current HTTPS tunnel origin; configured locally |
| `ENRO_TEST_PHONE` | Fixed authorized receiving phone; configured locally |
| `TWILIO_ACCOUNT_SID`, `TWILIO_AUTH_TOKEN` | Existing account credentials; configured and authenticated |
| `TWILIO_FROM_NUMBER` | Purchased voice-capable caller number; configured locally |
| `GOOGLE_CLIENT_ID`, `GOOGLE_CLIENT_SECRET` | Application OAuth credentials; still needed |
| `GOOGLE_MAPS_API_KEY` | Routes API key; still needed |

Google requires an OAuth **Web application** client, a permitted test account/consent setup, enabled Gmail and Calendar APIs, and the exact `${PUBLIC_BASE_URL}/oauth/google/callback` redirect. Scopes are `calendar.events.readonly` and `gmail.readonly`. Routes requires API enablement, provider billing, and a server-side key restricted to Routes. Connected chat plugins do not provide reusable OAuth credentials to this application. Follow [Google setup](docs/GOOGLE_SETUP.md) rather than trying to copy connector sessions.

Marketplace discovery did not find a suitable calling or calendar product, so the implementation uses documented direct APIs. Vercel CLI was available but is not part of the active runtime; deployment remains future work.

## Security and privacy boundaries

- `.env`, `.data/`, logs, dependencies, build output, and local deployment metadata are ignored. Do not force-add them for a team handoff.
- `.env` contains account secrets and full phone numbers. `.data/` contains access keys, Google tokens when connected, private event/call state, and local screenshots. Share required access separately through an appropriate private channel.
- Local bootstrap supplies the operator's admin token only to direct local requests. Admin APIs require that separate token. Remote credential editing is blocked even with admin authorization.
- The sensor URL contains a scoped ingestion secret. It does not grant admin access, but should still stay private. Valid sensor requests are rate-limited and size-bounded.
- Twilio callbacks require signature validation and matching account/call identity. OAuth uses expiring, single-use state.
- Call recording is disabled. The application can retain a short recognized speech string and per-call motion summary; “no audio recording” does not mean “no call metadata.”
- Incoming event text is data. It must not authorize calls to new recipients, credential changes, purchases, or unrelated actions.

This is a prototype boundary, not a production security certification. Do not claim multi-user isolation, mobile background guarantees, or recovery across multiple server replicas.

## Validation and claims we can make

The build's recorded validation is **58 passing automated tests**, TypeScript typecheck, and compilation. Provider tests use injected fixtures. HTTP integration tests start the actual server in isolated temporary storage without provider credentials. The later readiness-label change passed typecheck and JavaScript syntax checks. This documentation handoff does not claim a new full-suite run.

Recorded browser checks covered desktop and a 390px mobile layout, disabled unconfigured actions, no overflow, and no console errors. Earlier public-tunnel checks returned health 200, remote bootstrap 403, and unauthenticated state 401. These are scoped observations, not a penetration test or deployment uptime guarantee.

Useful commands for subsequent code changes:

```sh
npm run typecheck
npm test
npm run build
node --check public/app.js
```

**Do not claim yet:** a real wake-up succeeded; real motion was collected during a call; the phone was locked and continued streaming; Google data was imported; live travel was calculated; a scheduled call fired; natural-language email understanding or an LLM conversation is working; the system proves wakefulness; the product can run reliably overnight.

## Next work in order

1. **Finish the physical sensor setup.** Confirm real sample counts/timestamps and stationary → moving → stale transitions. A successful HTTP test-push proves connectivity only; it is not a recording or a passed movement test.
2. **Place one authorized real test call through Enro.** Answer, walk with the phone for several seconds, and say “I am up.” Preserve the carrier ID/status, answered/finished times, fresh sensor timestamps, latency, and result. Verify stop or keypad 0. Do not retry an uncertain request.
3. **Repeat with the phone locked before ringing.** If the stream stops, report the missing evidence and investigate the collector lifecycle. Two short successful trials still do not prove overnight behavior.
4. **Connect Google runtime credentials and authorize read access.** Import a real test calendar event and/or structured invitation; explicitly confirm attendance where required.
5. **Connect Routes and calculate a real plan.** Use a real origin/destination, travel mode, preparation time, and buffer. Verify timezone handling and then an event-time/location change. Any new billing authorization must be explicit.
6. **Exercise scheduling with a near-future test event.** Arm deliberately, observe one wake/leave dispatch, test cancellation/replanning, and confirm no duplicate calls. Avoid reboot/reset experiments during an uncontrolled active call.
7. **Rehearse and submit.** The working feature-freeze target was 5:45 PM Pacific, with submission at 6:30 PM. Choose the final track, disclose what is incomplete, include coding-tool use as required by the guide, and preserve a truthful fallback demo.

If simultaneous motion/calling cannot be made reliable, retain event-aware calling and spoken check-in but explicitly drop the movement-verification claim. A recorded demonstration, fixture, or browser audio is not an interchangeable substitute for a real live call; label any fallback clearly.

## Demo and pitch plan

Proposed opening: **“Your calendar knows when you need to arrive. Enro works backward and calls you when it is time to get moving.”**

Use one event and one person. The intended sequence is: show the real source → change its time or venue → show revised wake/leave calculations → ring the physical phone → explain the commitment and items to bring → walk and check in → show the call ending with timestamped evidence. The operator display explains the mechanism while the participant uses voice and movement.

Illustrative arithmetic only: an 8:30 AM event, 25-minute journey, 45-minute preparation, and 10-minute buffer imply **7:10 wake / 7:55 leave**. Do not present the 25 minutes as a measured Westfield UTC route. The application has no general accelerated-clock mode; use deliberately scheduled near-future test events or clearly disclose use of the separate test-call control.

Rubric story: everyday impact is avoiding missed commitments; innovation must come from the demonstrated event-to-action and physical-evidence loop; scalability needs an honest discussion of collector setup, call duration/cost, Routes usage, account permissions, and future hosting. No user-demand study, missed-event reduction, or market-first claim has been established.

Explicit cuts: toothbrushing recognition, sleep-stage inference, a custom native app, app-store launch, multiple users/households, broad inbox ingestion, arbitrary life-assistant behavior, unlimited nagging, real snooze rescheduling, and production deployment. Add sophistication only after the central live path works.

## Commit timeline and source documents

| Commit | Checkpoint |
| --- | --- |
| `a4faa61` | Existing initial repository |
| `bdcbc50` | Hackathon research and chosen wake-agent concept; first research snapshot published |
| `80e2928` | Wake-call prototype, motion, Google providers, travel scheduling, console and tests |
| `3ec6292` | Precise calling readiness and authenticated Twilio setup record |
| `69b8b88` | Approved voice-number purchase/configuration recorded |
| `e1e635d` | Detailed prototype team handoff and corrected local status documents |

This handoff follows those checkpoints. Initial local tracking information showed the build commits ahead of `origin/main`, but a fresh fetch revealed the separate team commits described above. The prototype history is preserved on `prototype-handoff-2026-10-24`; the team's `main` history is preserved separately. Use Git history and an updated remote to establish what a teammate has actually fetched.

Further context: [README](README.md), [phone test](docs/PHONE_TEST.md), [Google setup](docs/GOOGLE_SETUP.md), [service readiness](.hackathon/SERVICES.md), [build plan](.hackathon/MASTER_PLAN.md), [wake concept](.hackathon/WAKE_AGENT.md), [SIC shortlist](.hackathon/SIC_SHORTLIST.md), [earlier ideas](.hackathon/IDEAS.md), [job-agent alternative](.hackathon/JOB_AGENT.md), and [Matthew Kim research](.hackathon/MATTHEW_KIM_RESEARCH.md).

Concept and research documents intentionally preserve what was proposed at the time. Statements such as “no implementation” or older candidate recommendations in those historical files do not override this dated build snapshot. Update this handoff with observed evidence after the physical-phone and Google tests, rather than silently converting planned behavior into a success claim.
