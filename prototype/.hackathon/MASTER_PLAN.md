# Enro build

October 24, 2026. Submission deadline: 18:30 America/Los_Angeles. Target feature freeze: 17:45; preserve remaining time for a real-phone rehearsal.

Build one live path: Sensor Logger native HTTP push → fresh motion evidence → real Twilio phone call → spoken confirmation and motion-aware response. Then add read-only Google Calendar/Gmail, Google Routes, and explicit wake/departure scheduling.

Current status: native streaming, call transport, and spoken response worked at 4:35 PM Pacific on October 24; automatic completion took 23 seconds. This did not validate walking. The first attempt reached voicemail; see [physical-test evidence](../docs/PHONE_TEST.md). The 4:57 PM call was a **false positive**: the user did not walk, shook the phone vertically seven times during a speaker presentation, then said “I'm up.” The old pedometer-plus-voice gate counted 12→19 and ended after 39.5 seconds. Earlier walking-success claims are superseded. Do not place more calls during the presentation.

The revised experimental gate requires six fresh post-answer pedometer increments, fresh post-answer Activity classified as `walking`/`running`, and a spoken check-in. Missing, stale, stationary, or unknown Activity fails closed. Both sensors may still be fooled; controlled real-walk-versus-shake, pickup-only, locked-screen, and stop-command trials remain. The 113-test/typecheck/build result predates this correction and is software evidence only. Personal Google runtime OAuth and live sync passed; Routes billing/key remains deferred. Twilio credentials and the voice number are configured with user approval. Keep private identifiers in ignored configuration. Connected chat tools are not runtime OAuth. The [dated team handoff](../TEAM_HANDOFF.md) records an earlier snapshot; the corrected phone-test record supersedes its physical-walking claims.

Marketplace discovery returned no relevant phone-calling or calendar product (messaging only Resend, productivity empty). Use documented direct APIs. No fabricated calls, route estimates, sensor readings, or live integration success. Sensor Logger documentation supports native background push, but simultaneous locked-phone calling remains unverified.

Architecture: local TypeScript/Express server with durable local JSON state and a temporary HTTPS tunnel for callbacks and native sensor push. Native sensor collector handles OS access. Admin dashboard for test control and evidence; end-user flow is voice/motion. This requires the computer/server to stay running.

Physical acceptance: after the presentation, compare actual walking against stationary, pickup-only, and vertical-shaking trials using the same spoken check-in; include a locked-screen trial and stop/zero cancellation. Record both sensor channels and tester ground truth. The revised gate needs six post-answer increments plus fresh post-answer walking/running Activity plus speech. Acceleration freshness is five seconds; pedometer freshness is fifteen seconds. Neither a passing fixture suite nor sensor agreement proves walking or wakefulness.

Cut list: toothbrushing classification, sleep inference, custom native app, broad life assistant, multi-user service, app-store release. No automatic retries of an uncertain outbound call. All test fixtures are isolated from runtime state.
