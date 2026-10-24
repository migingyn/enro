# Privacy

> Ontime everytime.

Scopes, what is stored, and what is not.

Copied from `PRD.md` at kickoff. PRD.md is the source of truth. Edit here only to add what the PRD doesn't cover.

### 13. Privacy and safety

- OAuth read-only scopes only. No computer access.
- Store extracted events, never email bodies.
- Refresh tokens stored server-side only.
- Emergency contacts are messaged only after consent. Enforced and tested.
- Driving mode is sparse. Only reroutes and arrival time.
- Google app runs in testing mode with listed test users. Production would need Google verification for Gmail scopes.
