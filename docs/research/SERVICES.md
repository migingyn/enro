# Service readiness

October 24, 2026. No physical-phone test has passed yet.

| Component | State | Evidence / gap |
|---|---|---|
| Local TypeScript/Express runtime | Running | Local console and health endpoint serve; private files are ignored. |
| Temporary HTTPS tunnel | Reachable | Public health returned 200; public bootstrap blocked with 403 and unauthenticated state blocked with 401. |
| Native motion receiver | Implemented, isolated HTTP verified | Valid native-format requests are ingested; replay/staleness rejected. A real phone has not streamed yet. |
| Sensor Logger collector | User setup pending | Official app supports background HTTP push; actual concurrent phone-call behavior must be tested. |
| Twilio voice | Authenticated; calling number configured | User-authorized credentials saved only in ignored local configuration. User completed the compliance profile and approved $1.15/month plus taxes. One US local voice number was purchased, verified through the API, and saved locally. No real call placed yet; awaiting the phone motion stream. |
| Google Calendar/Gmail | Implemented, not authenticated | Read-only OAuth and request/parsing tests pass. Chat connectors do not supply application runtime credentials. |
| Google Routes | Implemented, not authenticated | Traffic/transit request tests pass. Requires API key, API enablement, and provider billing configuration. No live route verified. |
| Scheduling | Implemented, isolated tests passed | Wake/leave arithmetic, stable jobs, crash uncertainty, missed-job behavior, and replanning protections. Plans start disarmed. |
| Console | Browser verified | Desktop and mobile layouts, disabled unconfigured actions, local auth, timezone preview, no console errors. |

Validation: 58 tests pass, including a real local HTTP server in isolated temporary storage with no provider credentials. Typecheck and build pass. Fixture sensor ingestion is not a physical-phone test. Runtime state currently contains no call attempts or imported private events.

Variables: `PORT`, `PUBLIC_BASE_URL`, `ENRO_TEST_PHONE`, `TWILIO_ACCOUNT_SID`, `TWILIO_AUTH_TOKEN`, `TWILIO_FROM_NUMBER`, `GOOGLE_CLIENT_ID`, `GOOGLE_CLIENT_SECRET`, `GOOGLE_MAPS_API_KEY`. Values belong only in `.env` or the local setup form. Tokens and audit state are in ignored `.data/` files, with restrictive permissions.

Marketplace discovery: messaging returned Resend only; productivity returned no products. Neither meets the phone-calling or Google read-access need, so the app uses documented direct APIs. Vercel CLI 64.5.0 was detected; upgrade with `npm i -g vercel@latest` before a future Vercel deployment. The current runtime uses a local server and temporary HTTPS tunnel.

Reset: disarm plans and stop any active carrier call first. Stop the server, then move `.data/state.json` aside for a fresh test run; keep evidence if needed. Do not delete tokens or keys casually because that disconnects Google or invalidates the phone's push URL. Restarting the server marks previously active calls/dispatches unknown and requires reconciliation, preventing accidental retries.
