Do task S3 (config and seed data) from TASKS.md in the migingyn/enro repo. Read AGENTS.md, PRD.md (sections 5, 7, 10, 14, 15) and contracts/ first.

You own ONLY backend/config/** and backend/seed/**. Do not touch any other file. Another session is writing the C++ backend at the same time and its loaders expect EXACTLY the formats below, so do not rename fields. Use the contract field names from contracts/schemas/common.schema.json wherever an object is a User, EmergencyContact, Event, or Outcome. Timestamps are Unix epoch seconds. Timezone America/Los_Angeles.

The demo user must match the fixtures in contracts/examples/ exactly: id "u_demo", Sam Rivera, UC San Diego student, phone +18585550142, alarm 07:00, contacts Mom (+16195554417, CONFIRMED) and Jules (+18585550199, PENDING). Demo day is Monday 2026-10-26 (00:00 local = 1792998000). The demo-day events must use the same ids, titles, locations and times as contracts/examples/plan.json. Use only 555 fake phone numbers. No real people's data.

Files and formats:

1. backend/config/event_types.json
   {"types": {"class": {"base_prep_minutes": 20, "buffer_minutes": 5, "urgency_weight": 0.5}, "interview": {...}, "exam": {...}, "flight": {...}, "meeting": {...}, "casual": {...}},
    "urgency": {"weights": {"event_type": 0.4, "slack": 0.3, "load": 0.1, "deadline": 0.1, "bias": 0.1}, "levels": {"FIRM": 40, "URGENT": 70}},
    "mode_speeds_kmh": {"walk": 4.8, "bike": 15, "bus": 18, "drive": 35}}
   Per PRD 7.3/7.4. interview/exam/flight high weight, class/meeting medium, casual low. interview base_prep 45, buffer 15 (must match plan.json reasoning).

2. backend/config/personas.json
   {"archetypes": [{"id": "coach"|"best_friend"|"drill_sergeant"|"calm_assistant", "name", "description", "warmth", "harshness", "humor", "verbosity" (0..1), "system_prompt": "...", "voice": {"tts_voice": "alloy", "instructions": "tone instructions for OpenAI TTS"}}]}
   Persona never threatens anything except calling emergency contacts (PRD 7.10). Add a "urgency_style" object per archetype: {"CHILL": "...", "FIRM": "...", "URGENT": "..."}.

3. backend/config/scripts.json  (canned lines used when there is no LLM key)
   {"lines": {"<key>": ["line with {name} {event} {minutes} placeholders", ...]}}
   Keys at least: wake_call_1, wake_call_2, wake_call_3, awake_check, awake_verified, leave_reminder_chill, leave_reminder_firm, leave_reminder_urgent, departure_check, snooze_grant, snooze_counter, snooze_refuse, transit_comment, transit_alert, transit_reroute, arrived, contact_sms, contact_added_sms, onboarding_done. 2-3 variants each. contact_sms follows PRD 7.7 (automated message from Enro, light tone, no private details beyond "they have something at 9", mentions reply STOP). Include the demo line "Last three times you said five, it was eleven."

4. backend/config/onboarding.json
   {"instructions": "...", "questions": [{"slot", "prompt"}], "slot_schema": {<slot>: {"type", "description"}}}
   Questions must match contracts/examples/onboarding_session.json. Slots cover PRD 5 onboarding step 2-3.

5. backend/seed/demo_user.json
   {"user": <User>, "contacts": [<EmergencyContact>]}   (same values as contracts/examples/onboarding_complete.json but Mom CONFIRMED)

6. backend/seed/demo_calendar.json
   {"events": [<Event>]}  Demo day events from plan.json, plus a realistic week of the student's calendar around it (Mon 2026-10-26 to Fri 2026-10-30): recurring CSE 110 MWF, a COGS 108 section, gym, one exam. source "calendar".

7. backend/seed/demo_emails.json
   {"messages": [{"id": "msg_1001", "from", "subject", "snippet", "received_ts", "expected_verdict": "IGNORE"|"MAYBE"|"PLAN_CHANGE",
                  "extract": null | {"event_id", "canceled": bool, "new_start_ts": int|null, "new_location": string|null}}]}
   About 15 messages: newsletters and promos (IGNORE), ambiguous ones (MAYBE), and 3 plan changes. One must be id "msg_1003", from cse110-tas@ucsd.edu, subject "Section canceled today", extract cancels evt_cse110_dis (matches contracts/examples/ws_decision.json). Store snippets only, never full bodies (PRD 13).

8. backend/seed/demo_history.json  (three weeks before demo day: 2026-10-05 .. 2026-10-23)
   {"outcomes": [<Outcome> + "date": "YYYY-MM-DD", "predicted_travel_minutes": int],
    "wake_ups": [{"date", "alarm_ts", "awake_ts", "calls_needed": 0..3, "snooze_requests": [{"requested_minutes", "actual_minutes"}]}],
    "channel_events": [{"ts", "channel": "call"|"earbud"|"text", "answered": bool}]}
   Generate it with a small script you then delete (don't commit the script), but the numbers must produce roughly what contracts/examples/punctuality.json shows: class prep_bias ~ +12 min over ~24 samples, walk travel_factor ~1.15, bus ~1.20, snooze_inflation for class ~2.2 (said five, took ~eleven, at least the last three times), call answer rate in the morning ~0.58. Make it noisy and believable, not constant.

9. backend/seed/trips/bus_trip_01.json  (one bus ride with a detour, PRD 7.12)
   {"trip_id": "bus_trip_01", "route": "201", "agency": "MTS", "origin": "...", "destination": "Qualcomm Building Q, 6455 Lusk Blvd", "scheduled_arrival_ts": <10:44 on demo day>,
    "planned_stops": [{"name", "lat", "lng"}],
    "points": [{"seq", "ts", "lat", "lng", "next_stop", "on_route": bool}],
    "detour": {"start_seq", "end_seq", "reason": "Road closure on Genesee Ave"}}
   About 34 points, roughly 20 to 40 s apart, starting ~10:13 on demo day, on real UCSD to Sorrento Valley coordinates. Seq 17 must match contracts/examples/ws_trip_tick.json (lat 32.8798, lng -117.2134, off route). Add backend/seed/trips/README.md describing the format, for T24 who will record a real trip.

Validate every JSON file parses (python3 -m json.tool). Then ship with /shipit (branch S3-config-seed, commit prefix "S3:"). If the gh CLI isn't available in this environment, push the branch and open the PR however you can, then report the PR URL.
