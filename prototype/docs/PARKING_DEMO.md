# Scripted parking direction call

This is a recording cue, not a GPS wrong direction detector. The parked car location is a demo assumption: **Gilman Parking Structure**. The regular app's live sensors, scheduler, and calls are unchanged. No main server restart is needed.

Preview without calling:

```sh
node --import tsx scripts/call-parking-demo.ts --preview
```

When recording is ready, place one call to the already configured test phone:

```sh
node --import tsx scripts/call-parking-demo.ts --call
```

Chat controls: when the user types **call**, run `--call`. When the user types **reset**, run the following command to start a fresh scripted call from the beginning:

```sh
node --import tsx scripts/call-parking-demo.ts --reset
```

Reset ends an active call only when its saved ID and carrier phone numbers match this dedicated demo. A new call starts after the carrier confirms the old one ended. Unknown carrier outcomes and unrelated active calls block a replay. Reset leaves the main app's sensor data, plans, scheduler, and call history alone. The default pause stays five seconds and speech stays at 1.25×.

The expanded scripted call opens with inactivity, the 6:30 PM La Jolla Crossroads appointment, and the parked car at Gilman. It now listens after the opening for "Oh, I forgot about that, okay." After that reply ends, it waits five seconds for walking and gives the wrong direction correction. It listens for the user's turn around reply, then asks whether the user is wearing the full suit required by the fictional invitation. After that reply ends, it plays the final right direction/walking confirmation and goodbye.

All three prompts are nested inside speech enabled Gather turns, so listening is already active if the user starts replying during a prompt. Each turn waits for one second of speech silence before advancing, rather than ending recognition at the first brief pause. A turn allows thirty seconds to begin replying; silence ends with a separate no reply message instead of advancing to success. The call has a three minute maximum. Use `--delay=15` for more walking time after the opening reply (zero to thirty seconds).

These are scripted conversation cues: any speech/keypad reply advances to the next stage. They do not classify the reply, verify a suit, or infer direction from GPS. Continuations use Twilio Labs' HTTPS Echo Twimlet, with only the fictional script encoded in the callback URL. All three continuation responses were verified via HTTPS POST; the main app and server do not need a restart.

The preceding two reply call did receive one speech callback, but had no listening turn after its opening. That missing turn and the too eager `speechTimeout=auto` were corrected. The updated three turn configuration is checked in software; live turn taking still needs the next recording attempt.

All spoken lines use the male voice `Google.en-US-Neural2-D` with SSML `<prosody rate="125%">`. The separate compatible voice is used for this demo so the main voice setting stays untouched. See [Twilio SSML support](https://www.twilio.com/docs/voice/twiml/say/text-speech).

Preview is the default. Calling requires `--call` or `--reset`; no automatic retries or call recording. The command checks for active main/carrier calls and imminent scheduled reminders. Carrier outcome metadata stays in ignored private local state, separately from the main call history. A carrier accepted call does not establish that its audio played correctly; listen to the actual call before claiming the recording passed.
