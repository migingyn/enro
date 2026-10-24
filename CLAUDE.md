@AGENTS.md

# Claude Code specifics

- Ship with `/shipit`. It runs checks, opens the PR, and auto-merges when it's clean. On a conflict it stops and works through it with the user.
- No `Co-Authored-By` or "Generated with Claude Code" lines in commits or PRs.
- Check existing code before writing: `make todos` shows open work, and `grep -rn "TODO(T07)"` shows a single task's stubs.
- When you need the shape of an API response or websocket event, read `contracts/examples/<name>.json`. Don't guess.
- The repo is public. Anything you commit or push is published.
