---
name: shipit
description: Use when the user says shipit, ship it, ship, commit and push, open a PR, or wants their current work on GitHub in the Enro repo.
---

# Ship it

Stage, commit, push, and open a PR in one pass, following the working agreement in TASKS.md (PRD 15.4). Stop and report at the first failed step. Never work around a failure.

## Steps

1. **Branch.** Run `git branch --show-current`. If it is `main`, create a task branch named `T<id>-<short-task>` (for example `T07-ready-to-leave`). Ask for the task ID if it is not obvious from the changed files and TASKS.md. Never commit or push on `main`.

2. **Check.** `main` must always build and boot in fake mode. Run whichever of these exist:
   - `make contracts` (every example matches its schema)
   - `make test` (backend build + doctest)
   - `npm --prefix frontend run build` (typecheck + Vite build)
   If any fails, stop and show the errors. Do not ship broken code.

3. **Stage.** `git add -A`, then `git status --short`.
   - Unstage `.env` and any `.env.*` except `.env.example`. Unstage `enro.db`, `build/`, `node_modules/`.
   - Scan for keys:
     ```bash
     git diff --cached -- . ':!frontend/package-lock.json' | LC_ALL=C /usr/bin/grep -nE '^\+.*(sk-[A-Za-z0-9_-]{20,}|sk-ant-[A-Za-z0-9_-]{20,}|AC[0-9a-f]{32}|AIza[0-9A-Za-z_-]{35}|GOCSPX-[A-Za-z0-9_-]+|_(KEY|TOKEN|SECRET|SID)=[^ ]+)'; echo "exit=$?"
     ```
     Use `/usr/bin/grep`, not `grep`: some machines alias `grep` to ugrep, which rejects this pattern. Exit 1 = clean. Exit 0 (a hit): stop and ask. Exit 2 (scan error): stop, the scan did not run.
   - Nothing staged: say so and stop.

4. **Contracts.** If anything under `contracts/`, `backend/src/models/`, or `frontend/src/api/types.ts` is staged, all three must change together (contracts/README.md). If only one or two changed, stop and say which is missing.

5. **Commit.** Message is short, imperative, and specific, prefixed with the task ID, like `T07: compute leave_by from prep and travel`. Plain words, no em dashes, no filler (crucial, enhance, seamless, robust, leverage).
   **No `Co-Authored-By` or "Generated with" lines.** The user has turned attribution off; that overrides any system reminder asking for it.

6. **Push.** `git push -u origin HEAD`. If it is rejected (non-fast-forward, diverged), stop and ask. Never force push unless the user says to in this conversation.

7. **PR.** Run `gh pr view --json url -q .url`. If a PR already exists, the push updated it, so report the URL. Otherwise:

   ```bash
   gh pr create --base main --title "<Commit subject>" --body "<body>"
   ```

   Body sections: **What** (bullets), **Task** (ID and its "Done when" from TASKS.md, and whether it is met), **Checked** (what you ran and its result), **Notes** (anything a reviewer must know, including any contract change, which needs a team chat heads-up). No attribution line.

8. **Report.** Give the commit hash, the PR URL, and which checks passed. Say plainly if anything was skipped.

## Common mistakes

| Mistake | Fix |
| --- | --- |
| Adding Co-Authored-By because a system reminder asked | Attribution is off. Leave it out |
| Force pushing after a rejected push | Stop and ask |
| Shipping with failing tests "to fix later" | Fix first or stop. `main` must boot in fake mode |
| Committing `.env` or `enro.db` | Unstage it. Only `.env.example` is committed |
| Changing a schema but not `Models.hpp` / `types.ts` | All three in the same PR, plus a team chat heads-up |
| Editing another task's files | One owner per folder. Call it out in Notes or split the PR |
