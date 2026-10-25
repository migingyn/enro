# Enro

Wake up calls and departure reminders that work backward from a real commitment. Enro combines event time, travel, preparation, and an arrival buffer, then uses a phone call and fresh motion evidence for a morning check in.

**Team starting here:** read [the build handoff](TEAM_HANDOFF.md) for its dated project context, architecture, and setup. The [corrected phone test record](docs/PHONE_TEST.md) supersedes earlier physical walking success claims: the 4:57 PM call was a shaking false positive. Do not place more calls during the speaker presentation.

This folder preserves the TypeScript phone prototype. The C++ team build lives at the repository root. Both are available in main, with separate schedulers and call state. See [the integration notes](../docs/INTEGRATION.md) before connecting them.

## Run

Requires Node.js 22.13 or later.

```sh
cd prototype
npm ci
cp .env.example .env
npm start
```

Open `http://localhost:3333`. The local console authenticates automatically. Credentials are entered through **Connections → Configure service credentials** and saved to the ignored `.env` file. Do not overwrite an existing `.env` when restarting.

In another terminal, run `npm run tunnel`. Save the resulting HTTPS URL in Connections → Configure service credentials as `PUBLIC_BASE_URL`. The tunnel carries Twilio callbacks and Sensor Logger data to the local server; both processes and this computer must remain running. A new quick tunnel changes the callback URL. This is a development setup, not an overnight production deployment.

## First real phone test

See [the phone test runbook](docs/PHONE_TEST.md). The calling path requires a Twilio account with a voice capable number, a verified destination on trial accounts, and the native [Sensor Logger](https://sensorlogger.app/) app streaming to the console's private push URL.

Call transport and concurrent sensor delivery worked in an earlier trial. Walking validation has not passed: the user later reported that seven vertical shakes, with no walking, produced seven pedometer increments and ended the old gate after "I'm up." After the presentation, compare real walking with stationary, pickup, and shaking trials, then test locked screen delivery and stop/zero cancellation. Automated tests do not establish physical walking, background reliability, or wakefulness.

## Events and travel

Connect a Google OAuth web client with read only Gmail and Calendar scopes; configure a Google Routes API key. [Google setup and travel assumptions](docs/GOOGLE_SETUP.md) lists the exact requirements.

- Calendar imports upcoming located events with source timezone. Declined, cancelled, all day, and unlocated events are excluded.
- Structured email invitations (ICS) import individual UTC/IANA events. Confirm attendance once before planning; updates to that invitation retain the choice. Cancelled or missing invitations disarm their plans. Other email text remains available for manual event confirmation; natural language extraction is not implemented.
- Route estimates come from Google Routes. Driving uses traffic aware refinement; transit respects scheduled departures.
- Wake time = event start − arrival buffer − travel − preparation.
- Plans start disarmed. Arming enables wake and leave calls only to the configured test number.
- Armed plans refresh calendar and travel every five minutes while the server runs. Changed or missing calendar events invalidate the old plan. Unknown call outcomes are never retried automatically.

Voice currently uses Twilio speech recognition and bounded command handling for check in, stop, and snooze consequences. It does not yet use an LLM for free form conversation. Snooze explains the consequence but does not schedule a callback.

## Evidence and privacy

The console displays sensor freshness, carrier status, plan calculations, and audit history. The revised experimental wake/test gate requires six fresh post answer pedometer increments, fresh post answer Activity classified as `walking` or `running`, and a spoken check in. Missing, stale, stationary, or unknown Activity fails closed. Counter increments are not proof of actual steps: the old counter plus voice gate failed on shaking, and both the pedometer and Activity classifier may still be fooled. A walking versus shaking validation of the revised gate remains pending. Gravity indicates possible phone handling; acceleration remains diagnostic. Optional GPS is off by default and never establishes the intended route. Sensor microphone capture and Twilio call recording are disabled. See the [step and departure experiment](docs/DEPARTURE_TEST.md), including isolated hypothetical scenario controls.

`.env` and `.data/` contain credentials, private events, call metadata, and local access keys; both are ignored by Git. The sensor token only allows sensor ingestion. Administrative APIs require a separate token, remote credential editing is blocked, and Twilio webhook signatures are validated. This is a single user prototype.

```sh
npm run typecheck
npm test
npm run build
```

Provider tests use isolated fixtures. The HTTP tests start the real server with temporary storage and no provider credentials. See [.hackathon/SERVICES.md](.hackathon/SERVICES.md) for verified integration status and remaining setup.

The [live context panel](docs/LIVE_CONTEXT.md) exposes phone orientation, gyroscope rotation, GPS accuracy/speed, and location relative to an editable Crossroads reference. Open `/console.html#whereabouts`. Precise phone GPS and pose ingestion have been verified; travel progress and wake classifier physical trials remain separate.
