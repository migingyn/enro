# Instructions for coding agents

Read by Claude Code (via `CLAUDE.md`), Codex, Cursor, and any other agent, local or in a cloud session.

Enro is a 24-hour hackathon project: a background AI agent that wakes you up, gets you out the door, and learns how late you really run. The full spec is `PRD.md`. The task board is `TASKS.md`.

## Before you write code

1. Find your task row in `TASKS.md`. If the prompt gave you a task ID, use it. If it didn't, ask.
2. Read the PRD sections the row points to, plus section 10 (data models) and section 11 (API).
3. Work on a branch named after the task: `T07-ready-to-leave`.

## Scope rules

- **Edit only the files your task owns** (the "Folder(s)" column). If you need a change in someone else's file or in a shared file (TASKS.md, "Shared files"), stop and say so in your report or PR Notes. Don't make the change.
- **Contracts are frozen** unless the task is about them. `contracts/`, `backend/src/models/`, and `frontend/src/api/types.ts` change together, in one PR, with a heads-up to the team (contracts/README.md).
- **Stay in fake mode.** Defaults are `VOICE_PROVIDER=sim`, `LLM_PROVIDER=fake`, `REFLEX_PROVIDER=rules`, `CALENDAR_SOURCE=seed`, `EMAIL_SOURCE=seed`. Your task must work and be tested with zero API keys, even if it also adds a real provider.
- **Never commit secrets.** Only `.env.example`, with fake defaults. Never commit `.env`, `enro.db`, or real phone numbers.

## Code conventions

- C++20 in the backend. Prefer simple, readable code. The team has to extend it fast.
- Stubs return fake but valid data and are marked `// STUB`, with a TODO tag: `// TODO(T07): compute leave_by per PRD 7.4`. When you implement one, delete both markers. `make todos` lists what's left, by task.
- Every module has a test file in `backend/tests/`. Add real doctest cases for what you build. Planner, model, and consent logic need tests (PRD 7.7: contact consent is enforced in code and tested).
- Every agent action writes a `Decision` row and broadcasts it to `/ws/dashboard`. Judges watch that feed.
- Frontend reads fixtures from `contracts/examples/` when `VITE_USE_MOCKS=true`. Build against the mocks first, then the live backend.
- Match the style of the surrounding code: comment density, naming, idiom.

## Definition of done

1. Your row's "Done when" is met. Show how in the PR.
2. `make test` passes and `make contracts` passes.
3. In fake mode with an empty `.env`, the backend boots and `make dev-fake` runs the demo flow.
4. No `// STUB` or `TODO(<your task>)` left in your files, unless the PR Notes explain why.

## Shipping

Use `/shipit` in Claude Code, or follow `.claude/skills/shipit/SKILL.md` by hand. It branches, runs checks, commits with the task prefix (`T07: ...`), pushes, opens a PR, and auto-merges when everything is clean. On a merge conflict, stop and resolve it with the human. Don't pick a side wholesale.

## Cloud session prompt template

Start one cloud session per task:

```
claude --cloud "Do task T07 from TASKS.md. Follow AGENTS.md. Work only in the files T07 owns. Test in fake mode, then /shipit."
```

Good cloud tasks are marked "Fake" in the **Cloud?** column. Tasks marked "Local" need keys, phones, a mic, or ngrok, so run those on a teammate's machine.
