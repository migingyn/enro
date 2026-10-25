# Step and departure experiment

**Correction:** the 4:57 PM physical trial was a shaking false positive, not a passed walking trial. The user reports seven vertical shakes and no walking before saying "I'm up." Do not place more calls during the speaker presentation. The revised gate below still needs physical validation.

The console includes a hypothetical **Crossroads → Price Center** card at `/#planner`. At the staged 6:00 PM start, the event is at 6:30 PM. The initial **20-minute travel time is a manual assumption**, with a 10-minute arrival buffer; it is not a measured route. This yields a 6:00 PM leave time and, with 45 minutes of preparation, a 5:15 PM wake time. Changing travel time recomputes both. A real plan still requires Google Routes.

The five scenario buttons exercise the same departure assessment used for real plans. They never alter the real clock, trackers, events, plans, call history, or scheduler. No phone call or calendar appointment is created by the demonstration.

| Signal | Meaning |
|---|---|
| Still phone at leave time | Prompt to leave; the person's posture and readiness remain unknown. |
| Pickup or shaking | Phone handling can create false pedometer increments: the physical shaking trial did. Rejection by the revised gate is not yet validated. |
| Six new counter increments | Pedometer output, which may come from shaking; not independent proof of actual steps, wakefulness, or departure. |
| Activity payload | The revised gate requires fresh post answer `walking`/`running` as well as counter increments and speech. Missing, stale, stationary, or unknown Activity fails closed. Only a later stationary label has been observed physically; Activity may also be fooled. |
| Repeated accurate GPS outside the origin | Phone outside the configured starting area; not proof of the intended route or arrival. |

## Wake call

Enable Pedometer, Activity, Gravity, and calibrated Accelerometer in Sensor Logger. Keep microphone, camera, and GPS recording off for the first experiment. HTTP Push retains its private existing URL and one second batch interval. Native pedometer updates may arrive less often than HTTP batches.

The test call button requires a fresh pedometer reading. After the call is answered, the **first post answer pedometer sample becomes the baseline**. Only subsequent cumulative counter increases count. The revised wake/test rule requires at least six fresh post answer increments, fresh post answer Activity classified as `walking` or `running`, and an affirmative spoken check in. Missing, stale, stationary, or unknown Activity must not complete this gate. The old counter plus voice rule failed on vertical shaking. Adding Activity is an experimental mitigation: both channels may be fooled, and no physical validation of the revised rule has passed.

Pedometer freshness is 15 seconds; acceleration/gravity freshness is 5 seconds. Corroborating Activity must belong to the current post answer counter baseline and fall within an experimental five second window of the latest counter reading. Event driven Activity updates can miss that window, so a real walk may still fail this conservative check. Counter decreases, implausible jumps, long feed gaps, replayed messages, and recording changes cannot reuse old progress. A stop command or zero always ends the call. The test remains bounded to at most two minutes. Leave calls require an explicit "I am leaving"/"on my way" or the prompted key one; "I am awake" does not report departure.

## Optional GPS

`preferences.locationEnabled` defaults to false. The independent, in memory location tracker ignores payloads while disabled and clears retained coordinates on disable. Explicit origin coordinates belong to the individual plan (`originCoordinates`); an address is never silently treated as a geofence. No coordinates are written to audit/call history.

Experimental geofence thresholds: 120-metre radius, reported horizontal accuracy at most 25 metres, three fresh distinct outside fixes spanning at least 10 seconds, same device/recording, fix age at most 45 seconds. The accuracy radius must lie entirely beyond the boundary. Missing or uncertain GPS is unknown. Being outside the origin never automatically cancels the departure reminder.

## Physical rehearsal

Resume after the presentation, with the tester ready. Do not initiate another call during the presentation.

1. Answer while stationary, raise the phone, say "I am up": the revised gate must not complete without the required sensor evidence.
2. Shake vertically without walking and give the same spoken check in. Record actual behavior, pedometer increments, and Activity labels. Any automatic walking confirmation completion is a failed experiment even if both sensor channels report walking.
3. In a separate real walking trial, wait for the post answer baseline, walk six additional steps, and check in. Verify fresh post answer `walking`/`running` Activity, both channels' timestamps, and automatic hangup. Record physical steps independently of the phone counter.
4. Repeat with the phone locked and check delivery latency. Record a separate stop command trial.

The 4:57 PM Pacific call on October 24 completed after 39.5 seconds with counter 12→19 and the spoken check in, but the user was shaking, not walking. The earlier success claim is superseded by this false positive result; see PHONE_TEST.md for preserved telemetry. Revised real walk versus shake, pickup only, controlled locked screen, and stop command trials remain pending. The actual Activity payload shape and a later `stationary` label were observed; the Activity classification at the failed completion was not established.

Schema: [Sensor Logger HTTP push](https://github.com/tszheichoi/awesome-sensor-logger/blob/main/PUSHING.md). Update timing: [Apple CMPedometer](https://developer.apple.com/documentation/coremotion/cmpedometer/startupdates(from:withhandler:)).
