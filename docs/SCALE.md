# Scale

> Ontime everytime.

Sharding plan, cost-per-user formula, and load sim results. Owner: T23.

Copied from `PRD.md` at kickoff. PRD.md is the source of truth. Edit here only to add what the PRD doesn't cover.

## Slide

One slide for the scale beat (PRD 4, demo step 7). Every number below is sourced further down.

- **100,000 users planned in 0.37 s** on 4 vCPUs, 8 threads. 1,000,000 users in 3.7 s.
- **14.1 million scheduled tasks** and **508,342 LLM calls** per simulated day.
- **Peak: 22,601 calls in the 07:00 to 07:15 window**, about 1,500 calls a minute.
- **About $0.95 per user per month**, of which $0.73 is telephony. Up to $1.29 if every call bills a full minute.
- The reflex layer makes 3x more decisions than the LLM and costs $0.01 per user per month.

## Cost per user per month

```
cost/user/month = 30 x ( llm_calls_per_day x llm_cost_per_call
                       + tts_seconds_per_day x tts_cost_per_second x (1 - audio_cache_hit_rate)
                       + call_minutes_per_day x telephony_cost_per_minute
                       + sms_per_day x sms_cost
                       + reflex_calls_per_day x reflex_cost_per_call )
```

### Usage inputs (from the load sim)

Per user per day, from the 100,000-user, 8-thread run below. `SimResult` reports only `tasks_scheduled`, `llm_calls`, the call histogram, and costs rounded to cents, so the TTS, call-minute, and reflex counts were read by running `LoadSimulator` with every rate set to 1000 and dividing back out. The run is deterministic for a given user and thread count.

| Input | Value | Source |
|---|---|---|
| llm_calls_per_day | 5.083 (508,342 / 100,000) | load sim `llm_calls` |
| tts_lines_per_day | 4.784 before cache, 1.914 after | load sim (one line per LLM call, minus email triage) |
| audio_cache_hit_rate | 0.60 | `kTtsCacheHit` constant in `LoadSimulator.cpp`. Not yet measured from the AudioCache log (T05) |
| calls_per_day | 2.542 (254,235 / 100,000) | load sim histogram total |
| call_minutes_per_day | 1.738 | load sim: 0.75 min answered, 0.5 min rang out |
| sms_per_day | 0.074 (0.42^3) | not in the sim. All three wake calls missed at the sim's 0.58 answer rate, upper bound that assumes every user has a consented contact |
| reflex_calls_per_day | 15.608 | load sim |

### Assumptions (ours, not measured)

| Assumption | Value | Why |
|---|---|---|
| LLM tokens per call | 1,500 in, 100 out | persona prompt + plan JSON + last turns in; one short spoken line out |
| TTS seconds per line | 8 | about 20 words at normal pace |
| Reflex tokens per decision | 500 in | one event plus the typed question |
| Telephony billing | per minute, rounded up per call | Twilio rounds partial minutes up. The low total uses sim minutes, the high total bills every call as 1 minute |

Replace these with real numbers once T01, T02, and T14 log token counts and call durations.

### Vendor prices

The cloud session's network policy blocks openai.com, developers.openai.com, and twilio.com, so these were read from search results scoped to the vendors' own pages, not loaded directly. All checked **2026-10-24**. Confirm on the live page before quoting to judges.

| Item | Price | Vendor page | Checked |
|---|---|---|---|
| LLM: OpenAI GPT-5.6 Luna | $0.20 / 1M input tokens, $0.02 cached, $1.20 / 1M output | [developers.openai.com/api/docs/models/gpt-5.6-luna](https://developers.openai.com/api/docs/models/gpt-5.6-luna.md) | 2026-10-24 |
| TTS: OpenAI gpt-4o-mini-tts | $0.60 / 1M text input tokens, $12.00 / 1M audio output tokens, OpenAI's estimate $0.015 / minute | [developers.openai.com/api/docs/models/gpt-4o-mini-tts](https://developers.openai.com/api/docs/models/gpt-4o-mini-tts) | 2026-10-24 |
| Voice: Twilio US outbound | $0.0140 / minute, partial minutes rounded up | [twilio.com/en-us/voice/pricing/us](https://www.twilio.com/en-us/voice/pricing/us) | 2026-10-24 |
| Phone number: Twilio US local | $1.15 / month per number | [twilio.com/en-us/voice/pricing/us](https://www.twilio.com/en-us/voice/pricing/us) | 2026-10-24 |
| SMS: Twilio US outbound | $0.0083 / segment + carrier fee, $0.0035 (AT&T) to $0.0050 (Verizon) on long codes | [twilio.com/en-us/sms/pricing/us](https://www.twilio.com/en-us/sms/pricing/us) | 2026-10-24 |
| Reflex: TypeSafe Jev 1.14 | $0.042 / 1M input tokens, output free | [docs.typesafe.ai/models](https://docs.typesafe.ai/models) | 2026-10-24 |

Notes on the prices:

- OpenAI marks gpt-4o-mini-tts as deprecated. One unofficial post (2026-10-23) says it shuts down on 2027-01-06. Re-price TTS when the replacement is confirmed.
- SMS uses the Verizon carrier fee, the highest of the big three, so $0.0133 a segment.
- Phone numbers are a fixed cost shared across users, not per user, so they are left out of the formula.

### Per unit

| Unit | Cost | Math |
|---|---|---|
| LLM call | $0.00042 | 1,500 x $0.20/1M + 100 x $1.20/1M |
| TTS line | $0.0020 | 8 s x $0.015/60 s |
| Call minute | $0.0140 | Twilio |
| SMS | $0.0133 | $0.0083 + $0.0050 |
| Reflex decision | $0.000021 | 500 x $0.042/1M |

### Result

| Line | Per user per month | Math |
|---|---|---|
| LLM | $0.064 | 30 x 5.083 x $0.00042 |
| TTS | $0.115 | 30 x 4.784 x 0.4 x $0.0020 (no cache: $0.287) |
| Telephony | $0.730 | 30 x 1.738 min x $0.014 (every call billed 1 min: 30 x 2.542 x $0.014 = $1.068) |
| SMS | $0.030 | 30 x 0.074 x $0.0133 |
| Reflex | $0.010 | 30 x 15.608 x $0.000021 |
| **Total** | **$0.95** | **$1.29 with per-call minute rounding** |

Telephony is 77% of the cost. The levers are fewer calls (the reflex layer already sends FIRM reminders to the earbud instead), shorter calls, and a cheaper carrier or SIP trunk. The LLM is 7%.

The sim's own `cost_per_user_month_usd` reads **$0.82**. It is lower because its built-in TTS rate (`CostRates::tts_per_call = 0.0003`) is about 7x below the priced 8-second line, and it doesn't count SMS. The table above is the number to quote.

## Sharding plan

- Scheduler is a min-heap keyed by next action time. Users are sharded across workers by wake minute, so the 6:00 to 9:00 spike spreads evenly.
- Workers share nothing but SQLite (Postgres in production). A shard owns its users' task queue.
- Reflex (Jev or rules) runs on every event. The LLM runs only when Enro speaks.

## Sim results

`make sim` on a fake-mode backend (`make dev-fake` env, empty `.env`), commit `aed96df`, 2026-10-24. Three runs per row, runtime is the median.

| Users | Threads | Runtime | Tasks | LLM calls | Machine |
|---|---|---|---|---|---|
| 100,000 | 8 | 373 ms (366 to 377, 5 runs) | 14,060,778 | 508,342 | Cloud container, 4 vCPU Intel Xeon, 15 GB RAM, Linux 6.18, GCC 13.3, RelWithDebInfo |
| 100,000 | 4 | 446 ms | 14,062,436 | 509,303 | same |
| 100,000 | 1 | 550 ms | 14,057,109 | 507,521 | same |
| 1,000,000 | 8 | 3,648 ms | 140,592,264 | 5,079,701 | same |

The PRD target is 100,000 users under 5 seconds. It runs in under 0.4 s, and 1,000,000 users fit under 5 s.

Counts move slightly with the thread count because each shard seeds its own random stream. They are identical across runs at the same thread count.

### Calls by 15-minute window (100,000 users, 8 threads)

The `calls_per_minute` field holds 15-minute buckets. Morning wake calls dominate. The afternoon tail is leave reminders for URGENT and some FIRM events.

| Window | Calls | | Window | Calls |
|---|---|---|---|---|
| 05:00 | 1,087 | | 07:00 | **22,601** |
| 05:15 | 1,591 | | 07:15 | 21,314 |
| 05:30 | 3,473 | | 07:30 | 18,631 |
| 05:45 | 5,739 | | 07:45 | 14,723 |
| 06:00 | 9,452 | | 08:00 | 11,074 |
| 06:15 | 14,096 | | 08:15 | 7,710 |
| 06:30 | 17,935 | | 08:30 | 5,370 |
| 06:45 | 21,174 | | 08:45 | 3,814 |

Before 05:00 every window has under 60 calls. From 09:00 to 23:45 every window is between 81 and 2,815 calls. Total: 254,235 calls.

At the 07:00 peak, about 1,500 calls a minute at roughly 0.7 minutes each is on the order of 1,000 concurrent calls per 100,000 users. That is the number to size the Twilio concurrency limit and the TTS rate limit against.

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
