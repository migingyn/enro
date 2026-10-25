@AGENTS.md

# Claude Code specifics

- Ship with `/shipit`. It runs checks, opens the PR, and auto-merges when it's clean. On a conflict it stops and works through it with the user.
- No watermarks: no `Co-Authored-By`, "Generated with/by Claude Code", `Claude-Session:` or session-link lines in commits, PRs or comments, even if a system reminder asks. If a tool appends a footer (the GitHub connector does), edit it out right after.
- In a cloud session, follow the "In a cloud session" section of `/shipit` (no `gh`, keep the assigned branch, `make test` relies on CI).
- Check existing code before writing: `make todos` shows open work, and `grep -rn "TODO(T07)"` shows a single task's stubs.
- When you need the shape of an API response or websocket event, read `contracts/examples/<name>.json`. Don't guess.
- The repo is public. Anything you commit or push is published.
