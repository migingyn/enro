# October 24 release

This release preserves the team build and imports the phone prototype into `prototype/`. It adds a public demo build, a refreshed repository front page, a visual preview, and integration notes.

## Public experience

The landing page opens a sample morning without signup. The demo room includes manual travel math, silent call previews, staged sensor scenes, a corrected physical test record, and the Lookahead studio. The React dashboard is available with mock fixtures and a visible preview banner.

The local phone prototype remains available for real Google, Twilio, routing, and sensor experiments. Its credentials, personal events, precise location, runtime tokens, and call metadata are excluded from this release.

## Validation

The release checkout passed TypeScript checking, 145 prototype tests, 10 demo model tests, the prototype build, the React production build, 29 contract examples, and the C++ suite with 177 cases. The fake mode end to end run passed all 15 checks, including route smoke tests, scheduling demo beats, trip replay, load simulation, and dashboard events.

The earlier physical test remains a false positive for walking. The software checks do not validate waking a person, reliable background delivery, or the revised walking classifier.

The public browser flow was checked through event selection, plan math, the simulated call, and departure with a travel delay. The Crossroads preview rejects shaking and accepts staged walking plus a spoken response. The Lookahead scene updates its arrival forecast. All of these use sample inputs. The public landing page and scripts return successfully without login; private API and operator pages are absent.
