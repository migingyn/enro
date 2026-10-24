# Build tasks

| Owner | Files | Contract / acceptance | Status |
|---|---|---|---|
| Main | src/types.ts, src/server.ts, src/store.ts, src/providers/voice.ts, package files, docs | Real-call lifecycle, signed callbacks, private config, durable jobs, integration checks | Implemented; Twilio authenticated and number configured; physical test awaits native phone stream |
| Core | src/core/**, test/core*, test/server.integration.test.ts | Typed plan arithmetic; motion validation, replay/staleness rejection; behavioral and isolated HTTP tests | Checks pass |
| Google | src/providers/google.ts, src/providers/routes.ts, src/providers/invitations.ts, provider tests | Read-only OAuth, calendar/email event extraction, real Routes API; no invented facts | Implemented; checks pass; live authorization pending |
| Dashboard | public/** | Setup, live motion/call evidence, plan creation and arm control; no fixture data presented as real | Desktop/mobile browser checks pass |

Shared types are owned by Main. All agents share this directory and must stay within their file ownership. No agents edit dependency manifests or secrets. Send required dependency names to Main. Main performs all integration checks and owns external calls.

Current software milestone: typecheck, build, and 58 isolated tests pass. Native phone streaming and the actual carrier call remain pending; no real-call success is recorded. HTTPS health returned 200; remote bootstrap returned 403 and unauthenticated state returned 401.
