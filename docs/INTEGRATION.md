# Bringing the builds together

This release adds the complete local phone prototype under `prototype/` beside the existing C++ and React build. It preserves the team backend, frontend, shared schemas, and provider defaults. The designer's original files remain in `prototype/design/landing/`.

The public site combines the landing page, sample morning, Crossroads scenario, sensor test explanation, Lookahead studio, and team dashboard. The dashboard is built with `VITE_USE_MOCKS=true`. Calls are simulated and accounts remain disconnected there.

## Runtime ownership

The C++ build at the repository root owns the team planner, scheduler, wake ladder, and dashboard contracts. The TypeScript prototype owns its live phone experiment and native Sensor Logger ingestion. They are separate running systems. This release does not translate their APIs or share their call state.

For the next integration, choose one scheduler and one call controller. The C++ build is the intended team core. Port or adapt the verified provider and sensing behavior into that core. Do not arm both schedulers for the same person.

## What can carry over

The sky design, fonts, wordmark, sample scenario, test runbooks, route assumptions, and phone test findings can be shared immediately. The prototype's tests document edge cases worth retaining when behavior is ported. Existing main already contains work on its own Google and Twilio providers; preserve that work and compare behavior before replacing it.

## What needs an adapter

| Area | Prototype | Team core | Required decision |
| :--- | :--- | :--- | :--- |
| Time | ISO timestamps | Unix seconds | Convert once at the boundary |
| Duration | Route seconds | Planner minutes | Preserve rounding and arrival buffer |
| Events | Per event plans, source account, confirmation | Shared event and day plan contracts | Map identity, cancellations, and attendance |
| Calling | Local call records and bounded TwiML | VoiceProvider, wake ladder, and conversation state | Use one controller and preserve carrier terminal states |
| Sensing | Private native HTTP push, freshness, device and recording scope | Team motion and location endpoints | Carry freshness and source scope into the core |
| Persistence | Private local JSON state | SQLite repositories | Pick one authoritative store |

## Evidence that must survive the handoff

The physical seven shake trial was a false positive. A counter increment is not proof of walking. Activity is additional evidence, not a validated guarantee. GPS outside an origin does not prove travel toward the intended destination. Phone orientation does not establish body posture.

Keep the [corrected physical test record](../prototype/docs/PHONE_TEST.md) and [departure experiment](../prototype/docs/DEPARTURE_TEST.md) with any port. The dedicated [parking call](../prototype/docs/PARKING_DEMO.md) is scripted conversation and must not be presented as measured direction detection.

## Private configuration

Credentials and personal data stay in ignored `.env` and `.data/` files. The public build copies only the landing page, demos, fonts, preview image, and React assets. It excludes the local console, event approval page, private APIs, real events, and call history. No private service key belongs in the Vercel demo.
