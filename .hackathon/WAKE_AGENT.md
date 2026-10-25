# Wake-up and departure agent — provisional concept

Updated October 24, 2026. Evaluation only; no implementation or phone test has been completed.

## Recommendation

Develop Kevin's chosen alarm idea around getting ready and leaving on time. The distinguishing behavior should be adapting to real commitments and checking observable progress, with voice as the everyday interface.

One-sentence demo: When a student's event email changes the time or venue, the agent recalculates wake-up and departure times, calls with the reason and required items, and responds to measured movement plus a spoken check-in.

The event's Invisible Interfaces theme explicitly includes voice, motion, sensors, and phone calls. The Consumer AI track is a natural fit for the broad everyday use case. No App is also plausible if the core experience truly requires no screen; the published rules do not explicitly exempt onboarding screens. Choose one track for submission.

Sources: https://luma.com/compiled-im3b ; rubric reproduced in the team's Hacker Guide tab at https://docs.google.com/document/d/1h76F7lYYbtquW_43lZpmp_dveMFyaB3AZ0_zXf7E6KA/edit?tab=t.bot260rv2uqv . The shared guide lists Scalability, Innovation, and Real world impact without weights.

## Product boundary

- Extract the event's date, time, time zone, location, and requirements. Email receipt time is not event time. Use accepted commitments, not every promotional invitation.
- Compute wake time = event start − travel − preparation − arrival buffer. Preparation and acceptable wake-time bounds are user preferences. Arithmetic and state transitions can be deterministic; AI interprets messages and handles conversation.
- Recalculate on a relevant event or travel update. Explain why a reminder changed.
- Observe phone movement/steps, ask a brief spoken check-in, and end the wake conversation when the agreed check is complete. Report evidence accurately, e.g. “steps detected; user confirmed up.” This does not prove wakefulness.
- Offer spoken snooze/stop and a bounded retry policy. Do not depend on an uninterruptible call. Keep a normal local alarm as a reliability fallback in a production version.
- A judge-facing evidence display can show source, calculation, call state, and sensor evidence. The user should not need that display.

Kevin confirmed that “UTC” means Westfield UTC in San Diego and that the location idea calculates when to wake up or leave based on travel time. Treat wake-up and departure timing as one continuous flow. Travel estimates and example times below remain illustrative until connected to actual route data.

## Memorable demo

Use an explicitly accelerated clock. Example times below are illustrative, not live route estimates.

1. Read a real test email: “Tomorrow, 8:30 AM at Westfield UTC. Bring your ID.” With an illustrative 25 minutes travel, 45 minutes preparation, and a 10-minute arrival buffer, calculate wake-up at 7:10 and departure at 7:55.
2. Change the event time or venue live. Show the source and revised schedule.
3. The real test phone rings. The agent explains the change and the required item. The participant answers through an earpiece or speakerphone without navigating a product screen.
4. Show the difference between a picked-up phone and actual step evidence. Ask for a spoken check-in; do not claim the system cannot be fooled.
5. End the call after the agreed check. Demonstrate a short departure reminder if the critical path already works.

Thirty-second version: change event → revised plan → actual phone call with explanation → participant walks and answers → evidence updates and call ends. This must use functioning data paths; the accelerated schedule should be disclosed.

## Feasibility and access

Required but not yet confirmed: participant's phone OS/model, sensor permissions and collector, email/calendar authorization or a real forwarded test email, calling service credentials/number, routing access if live travel is included. Do not assume credits or permissions.

A normal phone call does not expose the recipient device's motion sensors. The voice channel and sensor collector are separate integrations.

MathWorks confirms MATLAB Mobile can stream acceleration, orientation, angular velocity, and position. Its iOS background acquisition is limited to position; Android offers an opt-in background acquisition setting. This is a substantial constraint for simultaneous real calling and sensor collection.

https://www.mathworks.com/help/matlabmobile/ug/sensor-data-collection-with-matlab-mobile.html

Android restricts background accelerometer and gyroscope events; its guidance recommends foreground execution or a foreground service. Apple's live pedometer updates stop while an app is suspended. Documentation establishes constraints, not that our particular phone setup works.

https://developer.android.com/develop/sensors-and-location/sensors/sensors_overview

https://developer.apple.com/documentation/coremotion/cmpedometer/startupdates(from:withhandler:)

Gyroscope readings measure phone rotation. Steps are an estimate of movement, not proof that a person is fully awake. A phone in a pocket cannot reliably establish toothbrushing merely from its gyroscope. Leave toothbrushing recognition outside the minimum scope.

## Critical spike

Time box: 20 minutes once hardware and accounts are available.

Test the actual target phone with screen locked: initiate the authorized test call, answer it, walk with the phone, and verify timestamped sensor events reach the agent during the call. Verify that a stop request or the agreed movement-and-response check ends the call. Repeat once to catch lifecycle failures. Measure update latency rather than assuming it.

Pass condition: ringing/answering works in the intended phone state; live step/movement evidence arrives during the call within five seconds for the chosen demo; the agent reacts and terminates correctly in both trials. No result yet.

If it fails: retain the real event-driven phone call and spoken check-in; drop the claim of automatic movement verification. That reduced scope proves contextual scheduling and voice interaction, but loses the main physical-sensing distinction. Do not silently replace a phone call with browser audio or replayed sensor events.

## Provisional assessment (1–5, subjective)

| Dimension | Score | Reason |
|---|---:|---|
| Demo clarity | 5 | A phone rings for an explained reason; a person gets up; the agent reacts. |
| Usefulness | 4 | Missed commitments and preparation timing are understandable, but demand is not validated. |
| Technical substance | 4 | Real event parsing, rescheduling, telephony, and sensor state can be demonstrated together. |
| Distinctive behavior | 3 | AI conversational alarms with calendar context already exist; adaptive departure planning and corroborated physical progress must carry the distinction. |
| Rubric fit | 4 | Clear everyday need and natural interaction; scalable reach is plausible, while per-minute call cost needs measurement. |
| Feasibility | 2 | Phone lifecycle and concurrent calling/sensing are untested; the result of the spike should govern scope. |

These scores are judgments, not probabilities of winning.

Competitor evidence: WakeYa advertises conversational alarms, calendar context, and wake-up checks; its FAQ says it uses in-app audio rather than placing phone calls. These are the vendor's claims, not our validation. https://wakeya.app/

## Cut list

No toothbrushing classifier, sleep-stage inference, guarantees of wakefulness, unlimited nagging, persona marketplace, multiple household users, full inbox ingestion, or broad life-assistant scope. Build one real morning path first.

Lesson from past winners: make the technical mechanism visible to judges while keeping the user's interaction simple. The decisive proof is that changed source data changes the plan and observed behavior changes the agent's next action.
