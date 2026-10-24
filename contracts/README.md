# Contracts

Every boundary between backend and frontend (and between teammates) is defined here first.

```
schemas/   JSON Schema (draft 2020-12), one per API response, request body, and websocket message
examples/  one example per schema, same base name. Backend stubs return these. Frontend mock mode reads these.
examples/twiml/  TwiML returned by /twilio/* webhooks (XML, not schema-checked)
```

The same shapes exist in three places and must stay in sync:

| Where | File |
|---|---|
| JSON Schema | `contracts/schemas/*.schema.json` (shared types in `common.schema.json`) |
| C++ | `backend/src/models/Models.hpp` + `backend/src/models/Json.hpp` |
| TypeScript | `frontend/src/api/types.ts` |

## Index

| Route / channel | Schema + example |
|---|---|
| `GET /health` | `health` |
| any non-2xx | `error` |
| `POST /api/onboarding/session` | `onboarding_session` |
| `POST /api/onboarding/complete` | `onboarding_complete_request` → `onboarding_complete` |
| `POST /api/contacts/:id/confirm` | `contact_confirm` |
| `GET /oauth/google/start`, `/callback` | 302 redirects, no JSON body |
| `GET /api/users/:id/plan?date=` | `plan` |
| `GET /api/users/:id/consequences?delay=` | `consequences` |
| `GET /api/users/:id/punctuality` | `punctuality` |
| `POST /api/location` | `location_request` → `location` |
| `POST /api/demo/trigger` | `demo_trigger_request` → `demo_trigger` |
| `POST /api/demo/trip` | `demo_trip_request` → `demo_trip` |
| `POST /api/sim/load` | `sim_load_request` → `sim_result` |
| `WS /ws/dashboard` | `ws_plan`, `ws_decision`, `ws_call_state`, `ws_turn`, `ws_trip_tick`, `ws_sim_result` |
| `WS /ws/sim-call/:call_id` server→browser | `simcall_ring`, `simcall_say`, `simcall_hangup` |
| `WS /ws/sim-call/:call_id` browser→server | `simcall_answer`, `simcall_decline`, `simcall_speech` |
| `POST /twilio/voice`, `/twilio/gather`, `/twilio/sms` | `examples/twiml/*.xml` |
| `POST /twilio/status` | empty 204 |
| `GET /audio/:id.mp3` | `audio/mpeg` bytes |

Every websocket message uses one envelope: `{ "type": "...", "ts": <epoch seconds>, "data": {...} }`.

Conventions: timestamps are Unix epoch **seconds**; durations are integer **minutes** unless the
field name says otherwise (`latency_ms`, `runtime_ms`, `duration_s`); field names are `snake_case`.

## Additions beyond PRD section 10

These fields are not in the PRD structs but the API needs them:

- `User.prep_minutes` (casual/formal answer from onboarding)
- `LeavePlan.urgency_score` (0 to 100, PRD 7.3)
- `Decision.latency_ms` (T14 logs reflex latency)
- Response wrappers: `DayPlan`, `WakePlan`, `ConsequenceReport`, `PunctualityReport`, `TripTick`, `SimResult`

## How to change a contract

1. Post a heads-up in the team chat: which schema, which field, why.
2. In **one PR**, update all of:
   - `contracts/schemas/<name>.schema.json` (or `common.schema.json`)
   - `contracts/examples/<name>.json`
   - `backend/src/models/Models.hpp` and the matching line in `Json.hpp`
   - `frontend/src/api/types.ts`
3. Run `make contracts`. It validates every example against its schema.
4. Prefer adding fields over renaming or removing them. A new field nobody reads yet breaks nobody.
