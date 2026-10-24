# Real phone and motion test

Runbook for the first physical-phone trial against this repo's C++ backend (port 8080). Adapted from the TypeScript prototype's runbook (branch `prototype-handoff-2026-10-24`), which was **never run on a physical phone**. Nothing below has passed yet. Record what actually happens.

Owners: T02 (Twilio voice), S7 (motion ingest), T10 (wake ladder). Background: [`PROTOTYPE_LEARNINGS.md`](PROTOTYPE_LEARNINGS.md).

## 1. Expose the backend

1. `make backend` with `VOICE_PROVIDER=twilio` and your Twilio values in `.env` (never committed).
2. In a second terminal, `make tunnel` (ngrok to `:8080`). Copy the `https://` origin into `PUBLIC_BASE_URL` in `.env` and restart the backend.
3. Check `curl $PUBLIC_BASE_URL/health` returns 200 from outside your laptop (e.g. phone on cellular).

The tunnel is temporary. A new tunnel URL means updating `PUBLIC_BASE_URL`, the Twilio number webhooks, the Sensor Logger push URL, and any Google redirect URI that uses it. Laptop, backend, and tunnel must all stay running for the whole test.

## 2. Connect calling (Twilio)

1. In the Twilio Console, copy the Account SID and Auth Token into `.env` (`TWILIO_ACCOUNT_SID`, `TWILIO_AUTH_TOKEN`). Put the voice-capable number in `TWILIO_FROM_NUMBER` in E.164. Keep all of these out of chat and Git.
2. Trial accounts only call verified numbers (PRD 17). Verify every demo phone early. Don't upgrade or buy a plan just to get past an unexplained error.
3. On the number's configuration page, point webhooks at main's routes:

| Twilio setting | URL |
|---|---|
| Voice, "A call comes in" (POST) | `$PUBLIC_BASE_URL/twilio/voice` |
| Messaging, "A message comes in" (POST) | `$PUBLIC_BASE_URL/twilio/sms` |
| Call status callback (set per outbound call by T02) | `$PUBLIC_BASE_URL/twilio/status` |
| `<Gather>` action (set in TwiML by T02) | `$PUBLIC_BASE_URL/twilio/gather` |

4. Signature checks (`X-Twilio-Signature`) are computed over the exact public URL. If they fail behind the tunnel, check that `PUBLIC_BASE_URL` matches the URL Twilio called, scheme included.

Only call phones whose owners agreed to be called. Don't commit or paste real phone numbers anywhere in this repo.

## 3. Connect the phone's sensors (Sensor Logger)

Twilio and Sensor Logger are **separate channels**. A phone call does not expose the callee's accelerometer. The backend joins them by time.

1. Install [Sensor Logger](https://sensorlogger.app/) on the test phone.
2. Enable the normal (calibrated) **Accelerometer**, which has gravity removed. Its values are m/s² on iOS and Android. Don't use the raw/uncalibrated sensor (g on iPhone) and don't rescale by 9.80665. Gyroscope is optional and unused. Leave microphone, camera, and location collection off unless background execution requires location permission (see step 5).
3. Logger settings → **HTTP Push** → URL: `$PUBLIC_BASE_URL/api/motion/<MOTION_INGEST_TOKEN>` (S7's route; the token is a private ingest secret from `.env`). Paste it exactly. Never use `localhost` on the phone and don't append another path.
4. Keep the default **1 s batch interval**. Set the phone to **automatic date and time**. Samples older than 5 s or more than 2 s in the future are rejected, so clock drift breaks ingestion.
5. Background/locked behavior: follow the app's own guidance. Its author documents background location permission as needed for reliable background execution, even for other sensors. On Android, battery optimization can also stop delivery. Test the exact phone you'll demo on.
6. Start recording. Confirm with `curl $PUBLIC_BASE_URL/api/motion` (S7) that sample counts and timestamps advance. A successful one-off "test push" only proves connectivity, not recording.

Payload reference: [Sensor Logger HTTP push schema](https://github.com/tszheichoi/awesome-sensor-logger/blob/main/PUSHING.md), [units](https://github.com/tszheichoi/awesome-sensor-logger/blob/main/UNITS.md). Don't invent a `sensorlogger://config/...` deep link or put the push URL through an online QR generator. It contains the ingest token.

## 4. Acceptance steps

Run in order. Stop at the first failure and write it down.

1. **Stationary → moving → stale.** Phone flat on a table: `GET /api/motion` shows fresh, not moving. Walk with it for several seconds: moving. Stop recording: within ~5 s it goes stale/unavailable. Never "still moving".
2. **One authorized real call.** Trigger one call to the verified phone (demo trigger or the T02/T10 path). If the create request times out, **check the Twilio call log before trying again**. The first request may have succeeded.
3. **Walk + "I am up".** Answer, walk for several seconds, say "I am up" (or press 1). Success needs motion that arrives *after* the answered timestamp plus the spoken/keypad check-in.
4. **Stop.** On a second call, say "stop" or press **0**. Stop must win over any success check and end the call.
5. **Locked-screen trial.** Lock the phone before it rings, repeat step 3. Record whether samples keep arriving during the call, the latency, and any gap.

Evidence to keep for each call: Twilio call SID and final status; answered and finished times; motion sample timestamps inside the answered window; latency; voice outcome. Put it in the PR or `docs/DEMO.md` notes, not in screenshots that show phone numbers.

If the sensor feed stops during a call, the result is "missing evidence", not success. If simultaneous call + motion can't be made reliable, keep the event-aware call and spoken check-in and **drop the movement-verification claim**.

## 5. What not to claim

Unless you've watched it happen and kept the evidence, don't claim any of these:

- a real wake-up call succeeded end to end
- real motion was collected during an answered call
- the phone kept streaming while locked
- motion proves the person is awake (it proves the phone accelerated; a shaken phone looks the same)
- overnight reliability (two short trials are not that)
- that a recording, browser simulator (`/call/<id>`), or fixture is the same as a live call. Label any fallback as a fallback.
