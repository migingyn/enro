# Risks

> Ontime everytime.

What can go wrong on stage and the fallback for each.

Copied from `PRD.md` at kickoff. PRD.md is the source of truth. Edit here only to add what the PRD doesn't cover.

### 17. Risks

| Risk | Mitigation |
|---|---|
| Twilio trial only calls verified numbers | Verify all demo phones early |
| Google OAuth consent screen friction | Testing mode with team accounts added |
| Jev access or latency | Rules fallback behind the same interface |
| Phone network fails on stage | `VOICE_PROVIDER=sim` and recorded backup |
| Live transit feed down | Recorded trip replay |
| Judges ask "isn't this Google Maps" | Lead with punctuality model and negotiation |
| Privacy concerns | PRIVACY.md and one line in the pitch |

### 16. Timeline (24h)

| Hours | Goal |
|---|---|
| 0 to 2 | Skeleton builds. ngrok. Twilio number. Phone rings with hardcoded TwiML |
| 2 to 5 | TaskQueue loop, Ready-to-Leave, Consequence Engine with tests |
| 5 to 8 | Google OAuth, calendar poll, email triage on seed inbox |
| 8 to 11 | Wake ladder with real calls, contact SMS |
| 11 to 14 | Conversation, negotiation, persona lines |
| 14 to 16 | Voice onboarding end to end |
| 16 to 18 | Trip replay and commentator |
| 18 to 20 | Punctuality model on seeded history. Jev wired |
| 20 to 21 | Load sim. Dashboard polish. Feature freeze |
| 21 to 24 | Rehearse 5 times. Record backup video |
