# Enro build

October 24, 2026. Submission deadline: 18:30 America/Los_Angeles. Target feature freeze: 17:45; preserve remaining time for a real-phone rehearsal.

Build one live path: Sensor Logger native HTTP push → fresh motion evidence → real Twilio phone call → spoken confirmation and motion-aware response. Then add read-only Google Calendar/Gmail, Google Routes, and explicit wake/departure scheduling.

Current blockers: native phone streaming and the physical call test remain pending; Google runtime OAuth and Routes credentials are not yet available. Twilio credentials are authenticated and a voice number has been purchased and configured with user approval. User supplied the test number; keep it only in ignored local configuration. iPhone Mirroring identified an iPhone 15 Pro, but collector setup and concurrent calling/sensing remain unverified. Connected chat Gmail/Calendar tools are not application OAuth credentials. See [the dated team handoff](../TEAM_HANDOFF.md) for the complete state and remaining work.

Marketplace discovery returned no relevant phone-calling or calendar product (messaging only Resend, productivity empty). Use documented direct APIs. No fabricated calls, route estimates, sensor readings, or live integration success. Sensor Logger documentation supports native background push, but simultaneous locked-phone calling remains unverified.

Architecture: local TypeScript/Express server with durable local JSON state and a temporary HTTPS tunnel for callbacks and native sensor push. Native sensor collector handles OS access. Admin dashboard for test control and evidence; end-user flow is voice/motion. This requires the computer/server to stay running.

Gate: two actual calls with timestamped motion arriving during the answered call, including one locked-screen trial. Call must respond to stop and terminate. Sensor data older than 5 seconds cannot count as current movement. Never infer proven wakefulness.

Cut list: toothbrushing classification, sleep inference, custom native app, broad life assistant, multi-user service, app-store release. No automatic retries of an uncertain outbound call. All test fixtures are isolated from runtime state.
