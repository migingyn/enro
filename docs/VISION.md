# Vision

> Ontime everytime.

What Enro is, why it is new, and how it maps to the judging criteria.

Copied from `PRD.md` at kickoff. PRD.md is the source of truth. Edit here only to add what the PRD doesn't cover.

### 1. Summary

Enro is a personal AI agent that runs in the background and gets you where you need to be. It wakes you up, gets you out the door, guides you there, and learns how late you really run. You talk to it by voice. After a one-time setup it has no interface.

It reads your calendar and email to find what's coming, decides how urgent each thing is, and reaches you by phone call or earbuds with a personality that adapts to you. If you don't wake up, it escalates and alerts the people you chose.

### 2. Hackathon context

**Challenge** Invisible Interfaces. Help people sense, decide, and act without a traditional app or screen.

**Primary track** Track 4, AI-Powered Consumer Products for 1 Billion People.

**Secondary angles** Track 3 (Unsupervised). The agent runs all day and decides when to stop. Track 1 (Dispatch). Live transit GPS moves people where they need to go.

**Judging** Top 5 demo after scoring on scalability, innovation, and real world impact. Section 4 maps each criterion to a demo beat.

### 3. What makes it new

Leave-now alerts and AI wake-up calls already exist separately. Enro is different in four ways.

1. **Personal punctuality model.** It learns your bias per event type. You leave 12 minutes later than planned for class. You underestimate walks. Formal events take longer to get ready for. It corrects the plan before you make the mistake.
2. **It talks back with your own numbers.** "Give me five minutes" is checked against the Consequence Engine and your history. "Last three times you said five, it was eleven."
3. **It decides when to speak.** A fast reflex layer (Jev) filters a constant stream of calendar, email, transit, and location events. Most produce silence. The LLM only runs when Enro has something worth saying.
4. **Escalation reaches real people.** If you sleep through everything, your chosen emergency contacts hear about it.

### 4. Judging criteria map

#### Scalability
- Server-side agent. Users need no device beyond a phone that takes calls.
- Scheduler is a min-heap priority queue keyed by next action time. Workers shard users by wake minute.
- LRU caches for parsed emails, routes, and TTS audio cut API calls and cost.
- Jev on the hot path (about 50 to 150 ms per decision, vendor numbers, measure your own). LLM only when speaking.
- GTFS-realtime is a standard feed used by transit agencies worldwide. One pipeline covers many cities.
- **Demo beat.** Load simulation plans 100,000 synthetic users and shows runtime, calls-per-minute histogram, and cost per user per month.

#### Innovation
- Punctuality model, negotiation with real numbers, reflex layer deciding when to talk, live trip commentary.
- **Demo beat.** Show the learned biases and a live Jev decision feed on the dashboard.

#### Real world impact
- Lateness costs people classes, shifts, interviews, and trust. Transit riders lose the most to delays and detours.
- Calls work on any phone. No smartphone app required.
- **Demo beat.** One slide with two or three cited stats found during the event. Do not invent numbers.

### 5. User flow

#### Onboarding (one time, about 3 minutes)
1. User opens the website and taps Start. The agent speaks through the browser mic.
2. Voice interview. Name, phone number, home area, usual wake time and whether they set their own alarm, how long they take to get ready for casual vs formal events, preferred transport by trip type, how harsh they want it.
3. Emergency contacts by voice. User says each name and relationship. Agent reads it back with the number's last four digits and asks for confirmation. Agent warns that names must match what the user will say later.
4. Google sign-in with Calendar read-only and Gmail read-only scopes.
5. Agent picks a starting personality from the answers and says "I've got it from here." The page shows a single "You're set" state. No dashboard for the user.

#### Daily use (background)
1. **Night before.** Pull tomorrow's events and recent plan-changing emails. Compute wake time and urgency. Schedule the backup alarm call.
2. **Morning.** The user's own alarm rings first. If no awake signal by the grace window, Enro calls. Escalation ladder in 7.6.
3. **Before each event.** Ready-to-Leave Engine schedules a reminder at the right lead time with the right tone.
4. **Departure.** Enro confirms you left (location movement or verbal confirmation). Keeps nudging until it does.
5. **In transit.** Earbud updates in commentator style. Reroutes during delays.
6. **After each event.** Logs arrival minus start. Updates the punctuality model, transport preferences, and personality.
7. **Anytime.** Plan-changing emails (canceled class, moved interview) update the plan and may move tomorrow's wake call.

### 6. Scope

#### P0 must ship
- Voice onboarding in the browser that saves a user profile and emergency contacts.
- Google OAuth (Calendar and Gmail read-only) with polling and sync tokens.
- Email triage. Reflex filter, then LLM extraction of plan-changing events.
- Priority-queue scheduler driving all agent actions.
- Ready-to-Leave Engine and Consequence Engine.
- Outbound calls through Twilio with OpenAI-generated persona lines and TTS.
- Turn-based conversation with intent classification and snooze negotiation.
- Morning escalation ladder with repeat calls and emergency contact SMS.
- Punctuality model seeded with three weeks of fake history.
- Trip replay of one recorded bus ride with live commentary and one reroute.
- Judge dashboard with plan math, Jev decision feed, call log, transcript.
- Load simulation.

#### P1 should ship
- Personality adaptation from replies and outcomes.
- Transport mode learning by destination type and distance.
- Live GTFS-realtime feed for one agency.
- Routes and ETAs from Google Maps for walk, bike, drive, transit.
- Departure verification through a location beacon page.
- Friend ETA sharing ("Want me to tell Alex you're running late?").

#### P2 stretch
- Native companion app (permissions only) for sleep data, existing alarms on Android, contacts, background location.
- Streaming voice for calls (Twilio Media Streams to OpenAI Realtime).
- Earbud audio delivery through a native app.
- Multi-city transit.

#### Out of scope
- Full computer access. Use scoped OAuth only.
- Payments, social leaderboards.

### 18. Success criteria

- Voice onboarding produces a working profile with confirmed contacts.
- Wake ladder escalates across three calls and texts the contact in parallel.
- A dropped email changes the plan live.
- Trip replay commentary reroutes on a detour.
- Punctuality card shows learned biases.
- Load sim runs live.
- Each judging criterion has a demo beat and a doc.
