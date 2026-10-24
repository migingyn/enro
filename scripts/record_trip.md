# Recording a real bus trip for replay (T24)

> Ontime everytime.

The demo replays a recorded bus ride at 10x so the trip commentary (PRD 7.12) works without a live feed. The seed ships `backend/seed/trips/bus_trip_01.json` (S3). This is how to record a real one.

## Before you ride

- Pick an MTS route from UCSD toward Sorrento Valley (the 201/202 loop or the 237). Note the route number and the stop you get off at.
- Phone charged. Location set to precise. Screen-off GPS logging allowed for the app you use.
- A GPS logger that exports GPX or CSV with timestamps, for example "GPS Logger" (Android) or "Open GPX Tracker" (iOS). Log one point every 15 to 30 s.

## During the ride

1. Start logging at the stop, before the bus arrives.
2. Note the scheduled arrival time at your destination stop from the MTS app.
3. If the bus detours (construction, closure), write down the time it leaves and rejoins the route. That segment becomes `detour`. If no real detour happens, pick a stretch to mark as one afterwards. Say so in the PR.
4. Stop logging after you get off.

## Converting to the replay format

The format is documented in `backend/seed/trips/README.md` (S3). In short:

```json
{"trip_id": "bus_trip_02", "route": "201", "agency": "MTS", "origin": "...", "destination": "...",
 "scheduled_arrival_ts": 1793036640,
 "planned_stops": [{"name": "...", "lat": 0, "lng": 0}],
 "points": [{"seq": 0, "ts": 1793034780, "lat": 32.88, "lng": -117.23, "next_stop": "...", "on_route": true}],
 "detour": {"start_seq": 14, "end_seq": 21, "reason": "Road closure on Genesee Ave"}}
```

- `ts` is Unix epoch seconds. Shift the whole trip so it lands on the demo day (Monday 2026-10-26), keeping the gaps between points.
- `next_stop` is the next planned stop along the route at that point.
- `on_route` is false for points inside the detour.
- Round lat/lng to 4 decimals (about 10 m). Strip anything that points to your home: start logging at a public stop, not your door.

## Check it

```bash
python3 -m json.tool backend/seed/trips/bus_trip_02.json >/dev/null
make dev-fake   # then, in another shell:
curl -X POST localhost:8080/api/demo/trip -H 'Content-Type: application/json' -d '{"trip":"bus_trip_02","speed":10}'
```

The dashboard's TripReplay panel should move along the route and the commentator should speak at the detour.
