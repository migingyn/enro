# Demo

> Ontime everytime.

Beat-by-beat script with the judging criterion each beat proves. Owner: T22.

Copied from `PRD.md` at kickoff. PRD.md is the source of truth. Edit here only to add what the PRD doesn't cover.

### 14. Demo script (3 minutes)

1. **Hook (15s)** "You set an alarm, it rings, you ignore it. Nobody knows you're about to be late. Enro does."
2. **Onboarding (25s)** Voice-only interview on the laptop. Confirms "Mom, ending in 4417." Google sign-in. "I've got it from here."
3. **Wake ladder (40s)** Interview day. Teammate ignores call 1 and 2. Call 3 yells. Mom's phone gets a text in parallel. Teammate answers and asks for five minutes. Enro counters with his history.
4. **Email drop (20s)** Send "Section canceled today" to the demo inbox. Dashboard shows the reflex verdict and the plan shifting.
5. **Trip replay (40s)** Recorded bus ride at 10x over the room speaker. Commentary. Injected detour. Reroute.
6. **Learning (15s)** Punctuality card. "You leave 12 minutes late for class. I already adjusted."
7. **Scale (20s)** Run 100,000-user simulation. Runtime, histogram, cost per user.
8. **Close (5s)** "Enro. Ontime everytime."

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
