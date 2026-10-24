# Scale

> Ontime everytime.

Sharding plan, cost-per-user formula, and load sim results. Owner: T23.

Copied from `PRD.md` at kickoff. PRD.md is the source of truth. Edit here only to add what the PRD doesn't cover.

## Cost per user per month

Fill in from the load sim (`make sim`, T16) and current vendor prices. Do not invent numbers.

```
cost/user/month = 30 x ( llm_calls_per_day x llm_cost_per_call
                       + tts_seconds_per_day x tts_cost_per_second x (1 - audio_cache_hit_rate)
                       + call_minutes_per_day x telephony_cost_per_minute
                       + sms_per_day x sms_cost
                       + reflex_calls_per_day x reflex_cost_per_call )
```

| Input | Value | Source |
|---|---|---|
| llm_calls_per_day | TODO from sim_result.llm_calls / users | load sim |
| audio_cache_hit_rate | TODO | AudioCache log (T05) |
| telephony_cost_per_minute | TODO | vendor pricing page, date checked |

## Sharding plan

- Scheduler is a min-heap keyed by next action time. Users are sharded across workers by wake minute, so the 6:00 to 9:00 spike spreads evenly.
- Workers share nothing but SQLite (Postgres in production). A shard owns its users' task queue.
- Reflex (Jev or rules) runs on every event. The LLM runs only when Enro speaks.

## Sim results

| Users | Threads | Runtime | Tasks | LLM calls | Machine |
|---|---|---|---|---|---|
| 100,000 | 8 | TODO | TODO | TODO | TODO |

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
