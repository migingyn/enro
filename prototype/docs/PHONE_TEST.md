# Real phone and motion test

**Corrected physical result:** the 4:57 PM trial was a false positive. The user reports that he did not walk: during a speaker presentation he shook the phone vertically seven times and said "I'm up," and the old pedometer plus voice gate ended the call. Earlier claims that this trial validated walking are superseded. **Do not place more calls during the presentation.**

## Connect calling

1. Create or sign into a [Twilio account](https://console.twilio.com/). Complete email/phone verification and obtain a voice capable trial number. Trial restrictions can require verifying the receiving phone. Do not upgrade or buy a plan just to bypass an unexplained error.
2. In the Twilio Console, locate the account SID and auth token; copy them directly into Enro's **Connections → Configure service credentials** at `http://localhost:3333`. Enter the Twilio number in international format as the From number. Keep the token out of chat and Git.
3. The receiving number is `ENRO_TEST_PHONE` in the ignored `.env` file. Enro never accepts an arbitrary destination from a public request.
4. Run the local server and HTTPS tunnel. Set its current base URL in Connections → Configure service credentials. Twilio callbacks are supplied with each outgoing call; no manual inbound number webhook change is needed.

Sources: [Twilio voice trial](https://www.twilio.com/docs/usage/tutorials/how-to-use-your-free-trial-account), [Calls API](https://www.twilio.com/docs/voice/api/call-resource), [request validation](https://www.twilio.com/docs/usage/security).

## Connect the phone's sensors

1. Install [Sensor Logger](https://sensorlogger.app/) on the test phone.
2. Enable **Pedometer, Activity, Gravity, and Accelerometer** (gravity removed). Acceleration and gravity remain diagnostics; they do not establish walking. Leave microphone, camera, and GPS recording off.
3. Configure **HTTP Push** with the private URL copied from Enro's console. Use the default one second push interval and begin recording.
4. For background recording, follow the app's permission guidance. Its author documents background location permission as necessary for reliable background execution, including for other sensors. Enro's optional location collector is disabled for these trials and does not retain GPS samples while disabled. On Android, battery optimization may also affect delivery. Test the exact phone state; this is not an assurance of overnight operation.
5. Watch Enro's sensor panel. It should show real sample counts and timestamps. If no fresh data arrives, it must remain unavailable or stale.

Source: [Sensor Logger background collection](https://www.tszheichoi.com/sensorloggerhelp), [official HTTP payload schema](https://github.com/tszheichoi/awesome-sensor-logger/blob/main/PUSHING.md).

## Resume controlled trials after the presentation

The console's **Place real test call** button places an actual call to the configured phone. It does not simulate ringing. If a carrier request times out, inspect Twilio call logs before retrying; the first request might have succeeded.

1. After the presentation, with the tester ready, start sensor recording and keep the phone still for a few seconds before placing a test call.
2. Establish a post answer pedometer baseline. In a real walking trial, walk at least six additional steps with the phone and say "I am up." The revised gate requires fresh post answer counter increments **and** fresh post answer Activity classified as `walking` or `running` **and** the spoken check in. Missing, stale, stationary, or unknown Activity must not satisfy it.
3. Run separate stationary, pickup only, and vertical shaking without walking trials with the same spoken check in. Record the person's actual behavior alongside both sensor outputs. Any nonwalking trial that completes the walking gate is another false positive.
4. Repeat controlled trials with the screen locked before ringing. Record delivery latency and interruptions. Say "stop" or press `0` in a separate cancellation trial. All calls remain bounded to two minutes.

Required evidence: real carrier call ID/status; answer and finish timestamps; post answer pedometer baseline and increments; Activity classification and sample/receipt timestamps; latency; spoken outcome; and the tester's actual behavior. Transport success and sensor counters are not physical walking validation. Both the pedometer and Activity classifier may be fooled by handling; the revised gate is experimental, not proof of wakefulness or overnight reliability.

If the sensor feed stops during a call, the call must report missing evidence rather than claiming success. Address the phone collector's lifecycle before expanding the product.

## Current result

**The earlier call transport, sensor delivery, and speech response worked on October 24, 2026; walking was not validated.** Sensor Logger on the user's iPhone 15 Pro streamed the gravity removed accelerometer through one second HTTP Push batches. Approximate location access, including Always, was explicitly approved and enabled for background operation; the Location/GPS recording switch was visually verified off.

The first call reached voicemail at 4:29 PM Pacific. Motion continued, but no spoken check in succeeded. The user reported that the phone never rang. Phone notifications were observed off and the silent mode icon was visible; unknown caller filtering was not established as the cause. After the user said the phone should work and requested another attempt, the second call established transport and speech response:

| Evidence | Observed result |
| --- | --- |
| Carrier interval | 4:35:49 to 4:36:12 PM Pacific; 23 seconds; status completed |
| Recognized check in | "I'm up." |
| App result | `movement-and-voice-confirmed` |
| Post answer sensor evidence | 1,979 samples, movement in 13 distinct seconds |
| Last preserved sample latency | About 3.36 seconds, within the five second freshness limit |
| Termination | Automatic spoken confirmation and hangup; no console stop |

The stream remained fresh during and after both carrier call intervals. This establishes short call transport and concurrent sensor delivery, not walking, wakefulness, overnight reliability, or a separately controlled locked screen trial. Stop speech/keypad cancellation still needs a physical test. The first attempt also exposed a stop result race: a console stop after the call's time limit overwrote its already terminal result. The stop route now preserves terminal evidence, with isolated regression coverage.

Twilio authentication, number ownership, and voice capability were verified before testing. The user approved the purchased US local voice number at $1.15/month plus taxes. Credentials, full phone numbers, carrier IDs, and the transport test screenshot remain in ignored local files.

HTTP ingestion, freshness/replay rejection, voice state transitions, signatures, and scheduler behavior pass isolated automated checks.


## False positive: pedometer and voice call, October 24 at 4:57 PM Pacific

The old gate ended the call incorrectly for the intended walking condition. App call ID: `913a9c45-0435-4f60-a387-5dc987c2292b`. Answered at `2026-10-24T23:57:22.244Z`; carrier completion callback at `2026-10-24T23:58:01.723Z` (39.479 seconds later). Historical app result: `steps-and-voice-confirmed`. **User ground truth: no walking; seven vertical shakes during a speaker presentation, then "I'm up." The stored result label is a false positive, not a passed walking test.**

The first post answer pedometer baseline was 12 at `23:57:33.949Z`; the final cumulative count was 19, giving **7 counter increments despite no actual walking**. Five increments remained below threshold; seven increments alone still left the call waiting for a spoken check in. The final recognized statement included "I'm up." Final pedometer sample at `23:57:59.490Z` arrived at `23:58:00.073Z`, about 583 ms later. The call ended automatically, without a console stop. These timestamps and counts are retained as failure evidence.

Gravity also reported an orientation change. The user's correction establishes a shaking false positive in this trial; a separate pickup only rejection trial was not established. GPS ingestion stayed disabled. Real walking versus shaking, a controlled locked screen trial, and a stop command trial must be tested against the revised gate after the presentation. No new gate has passed physical validation.

Local screenshot evidence remains in ignored `.data/enro-step-call-success.png`; the legacy filename reflects the superseded assessment and does not establish success. New samples continued after the call. Activity's actual payload shape was observed as `name: activity`, `values.activity: string`; a later live label was `stationary`. That later observation does not establish the Activity classification at the false positive completion time. The revised requirement for fresh post answer `walking`/`running` Activity is unvalidated on a real walk, and both sensor channels may still be fooled.
