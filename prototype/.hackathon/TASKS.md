# Build tasks

| Owner | Files | Contract / acceptance | Status |
|---|---|---|---|
| Main | src/types.ts, src/server.ts, src/store.ts, src/providers/voice.ts, package files, docs | Real-call lifecycle, signed callbacks, private config, durable jobs, integration checks | Earlier transport/speech check worked; 4:57 PM walking claim corrected to shaking false positive. No more calls during presentation. Revised gate awaits physical validation. |
| Core | src/core/**, test/core*, test/server.integration.test.ts | Typed plan arithmetic; motion validation, replay/staleness rejection; behavioral and isolated HTTP tests | Checks pass |
| Google | src/providers/google.ts, src/providers/routes.ts, src/providers/invitations.ts, provider tests | Read-only OAuth, calendar/email event extraction, real Routes API; no invented facts | Personal Google live reads passed; university SSO and deferred Routes billing/key remain pending |
| Dashboard | public/** | Setup, live motion/call evidence, plan creation and arm control; no fixture data presented as real | Desktop/mobile browser checks pass |

Shared types are owned by Main. All agents share this directory and must stay within their file ownership. No agents edit dependency manifests or secrets. Send required dependency names to Main. Main performs all integration checks and owns external calls.

Historical software milestone: the original typecheck/build/58-test suite passed; the later stop-result race fix passed typecheck and 14 focused HTTP tests. Native streaming, actual carrier-call transport, “I'm up,” and automatic hangup worked at 4:35 PM; this was not walking validation. See docs/PHONE_TEST.md for evidence and the corrected later failure. HTTPS health returned 200; remote bootstrap returned 403 and unauthenticated state returned 401.

Pre-correction step/departure software milestone: 113 tests plus typecheck/build passed. Crossroads 6:00 PM → Price Center 6:30 PM remains isolated and hypothetical. The counter-plus-voice gate failed physically on vertical shaking. The revised experimental gate requires six fresh post-answer counter increments, fresh post-answer walking/running Activity, and speech; missing/stationary/unknown or stale Activity fails closed. Both sensor channels may be fooled. Departure evidence remains distinct from intended-route proof; GPS is off. See docs/DEPARTURE_TEST.md.

Corrected physical result: the 4:57 PM call was a false positive. Baseline 12→19 was seven counter increments while the user shook vertically seven times without walking. With “I'm up,” the old gate ended 39.5 seconds after answer. Preserve that telemetry; the old success label is superseded by user ground truth. A later Activity stationary label was observed, not the completion-time classification. No more calls during the presentation. Afterward, validate the revised gate with separate actual-walking, shaking, pickup-only, locked-screen, and stop-command trials.

Revised software verification: 116 tests, typecheck, and build pass. The new regression covers seven pedometer increments plus voice with stationary/missing/stale Activity. Corroboration must come from the current post-answer baseline/sequence and lie within an experimental five-second window of the latest counter sample. Native Activity is event-driven, so this strict window may reject real walking until tuned in physical tests. The live app labels the historical trial user-reported false positive while preserving its original machine result and timestamps. No calls were placed during this correction.

Live context milestone: native orientation, gyroscope, and precise GPS ingestion verified in the running app. Added editable place reference, local location diagram, GPS quality/movement summaries, and agent context. 126 software tests pass; no new phone calls were made.
