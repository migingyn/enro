# Google setup: Calendar, Gmail, Routes

For T03 (`backend/src/google/`, `http/OAuthRoutes.cpp`, `routing/GoogleRouter.cpp`). Adapted from the TypeScript prototype on branch `prototype-handoff-2026-10-24`. The prototype's provider logic was tested only against fake HTTP responses. **No live Google sign-in, import, or route has been verified yet.**

Fake mode needs none of this: `CALENDAR_SOURCE=seed`, `EMAIL_SOURCE=seed`, and an empty `GOOGLE_MAPS_API_KEY` (fixed speeds per mode).

## 1. OAuth client (Calendar + Gmail)

1. In Google Cloud Console, create or pick a project. Enable **Google Calendar API** and **Gmail API**.
2. Google Auth Platform → consent screen: **External**, publishing status **Testing**. Add every teammate and the demo account as **test users** (PRD 13, 17). Production use of Gmail scopes would need Google verification. Not in scope.
3. Create an OAuth client of type **Web application**:
   - Authorized redirect URIs, exactly as `GoogleAuth::redirectUri()` builds them (`<PUBLIC_BASE_URL or http://localhost:PORT>/oauth/google/callback`):
     - `http://localhost:8080/oauth/google/callback`
     - `https://<your-tunnel-domain>/oauth/google/callback` when `PUBLIC_BASE_URL` is set (re-register whenever the tunnel URL changes)
   - Authorized JavaScript origin: `http://localhost:5173`
4. Put `GOOGLE_CLIENT_ID` and `GOOGLE_CLIENT_SECRET` in `.env`. Never commit them or any token. Set `CALENDAR_SOURCE=google` and/or `EMAIL_SOURCE=gmail`.
5. Sign in at `/oauth/google/start`. The callback lands on the frontend `/done` page, or `/onboarding?error=google` on failure.

**Scopes** (from `GoogleAuth::kScopes` on main): `https://www.googleapis.com/auth/calendar.readonly` and `https://www.googleapis.com/auth/gmail.readonly`. Read-only only (PRD 13). The prototype used the narrower `calendar.events.readonly`, which is enough to list events. T03 can switch if it doesn't need calendar metadata.

Prototype lessons for T03:

- Validate a **random, single-use, expiring `state`** on the callback before exchanging the code. Main's TODO currently puts the `user_id` in `state`. Bind the user server-side instead, or at least add a nonce.
- Use `access_type=offline` and `prompt=consent` so a refresh token comes back. Refresh can still fail (revoked or expired). Surface "reconnect Google" rather than silently falling back to seed data.
- A chat assistant's Gmail/Calendar connectors are not app credentials. They can't be reused by the backend.

## 2. What to import (calendar and email rules)

- Calendar: primary calendar, next 7 days, recurring instances expanded (`singleEvents=true`), source timezone preserved. Exclude all-day, cancelled, self-declined, past, and unlocated events. Keep tentative events marked tentative, not as confirmed commitments.
- Gmail (T09 triage feeds off this): an email's **receive time is never the event time**. Event date, time, timezone, and location must come from the content or a structured invite. Structured one-off ICS invites with an unambiguous UTC/IANA start, title, and location are safe to normalize. Recurring, floating, all-day, ambiguous-DST, or incomplete invites need confirmation. Per PRD 7.2, store extracted facts, never bodies.
- Bounded imports (the prototype read 10 candidates): a message missing from a later bounded fetch is **not** proof of cancellation. With `historyId` sync (PRD 7.2) this matters less, but don't treat absence as a cancel.

## 3. Routes API (travel time)

1. Enable **Routes API** in the same project. **Billing must be enabled** on the project; Routes is a paid Maps Platform SKU. Any new billing authorization should be explicit and agreed by the team.
2. Create an API key and restrict it: API restriction **Routes API only**. It's server-side only (`GOOGLE_MAPS_API_KEY`), never shipped to the frontend.
3. Endpoint: `POST https://routes.googleapis.com/directions/v2:computeRoutes` with `X-Goog-Api-Key` and a field mask (see the TODO in `routing/GoogleRouter.cpp`).

If the key is missing, billing is off, or no route exists, fail visibly in the reasoning line. Don't guess a duration silently. Main's fallback is fixed speeds per mode when the key is **empty**. Keep that, but don't use it to hide a failing real request.

### Travel-mode nuances (from the prototype)

The request should target arrival at `event.start − buffer` (PRD 7.4), then `leave_by = arrival − travel`, `reminder_at = leave_by − prep`.

- **DRIVE**: Google ignores `arrivalTime` for driving. Query traffic near the target arrival, then refine up to twice with `departureTime = arrival − last estimated duration`. **Keep the longest duration seen.** That's up to 3 billable calls per estimate. It's a conservative approximation, not an arrival-time solver.
- **TRANSIT**: request with `arrivalTime`. Take departure from the **first scheduled transit leg's departure minus the walking time before it**, not `arrival − duration`. Subtracting a nominal duration can miss an infrequent bus. Missing itinerary details are an error, not a guess.
- **WALK**: use the returned duration as-is, no traffic preference.
- **Freshness**: reject route estimates older than **15 minutes** for planning. Recompute on refresh rather than reusing an old route. T05's route cache TTL should not exceed this.

Sources: [OAuth web-server flow](https://developers.google.com/identity/protocols/oauth2/web-server), [Calendar events.list](https://developers.google.com/workspace/calendar/api/v3/reference/events/list), [Gmail messages](https://developers.google.com/workspace/gmail/api/reference/rest/v1/users.messages), [computeRoutes](https://developers.google.com/maps/documentation/routes/reference/rest/v2/TopLevel/computeRoutes), [Transit routes](https://developers.google.com/maps/documentation/routes/transit-route).
