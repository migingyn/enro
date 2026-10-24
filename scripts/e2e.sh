#!/usr/bin/env bash
# Headless golden path in fake mode (PRD 14). Boots the backend with every provider faked and a
# throwaway DB, runs the route smoke test, then drives the demo beats and checks the dashboard
# websocket saw them. Usage: scripts/e2e.sh   (or `make e2e`). Needs: curl, python3, jsonschema.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${BUILD:-build}"
PORT="${PORT:-18080}"
BASE="http://localhost:$PORT"
BIN="$ROOT/$BUILD/enro"
TMP="$(mktemp -d)"
LOG="$TMP/backend.log"

pass=0
fail=0
ok()  { echo "  PASS  $1"; pass=$((pass + 1)); }
bad() { echo "  FAIL  $1"; fail=$((fail + 1)); }

cleanup() {
  [[ -n "${WS_PID:-}" ]] && kill "$WS_PID" 2>/dev/null || true
  [[ -n "${PID:-}" ]] && kill "$PID" 2>/dev/null || true
  wait 2>/dev/null || true
  rm -rf "$TMP"
}
trap cleanup EXIT

[[ -x "$BIN" ]] || { echo "no backend binary at $BIN. Run: make build"; exit 1; }

echo "== boot (fake mode, port $PORT)"
ENRO_ENV_FILE=/dev/null PORT="$PORT" DB_PATH="$TMP/enro.db" DEMO_MODE=true \
  VOICE_PROVIDER=sim LLM_PROVIDER=fake REFLEX_PROVIDER=rules CALENDAR_SOURCE=seed EMAIL_SOURCE=seed \
  "$BIN" >"$LOG" 2>&1 &
PID=$!
for _ in $(seq 1 50); do
  curl -sf "$BASE/health" >/dev/null 2>&1 && break
  kill -0 "$PID" 2>/dev/null || { echo "backend exited during boot:"; cat "$LOG"; exit 1; }
  sleep 0.2
done
curl -sf "$BASE/health" >/dev/null && ok "/health 200" || { bad "/health"; cat "$LOG"; exit 1; }

# Record every dashboard event while the demo beats run. Skipped if websocket-client is missing.
WS_OUT="$TMP/ws.jsonl"
if python3 -c "import websocket" 2>/dev/null; then
  python3 - "$PORT" "$WS_OUT" <<'PY' &
import json, sys, websocket
port, out = sys.argv[1], sys.argv[2]
ws = websocket.create_connection(f"ws://localhost:{port}/ws/dashboard", timeout=30)
with open(out, "a") as f:
    while True:
        try:
            msg = ws.recv()
        except Exception:
            break
        f.write(msg.strip() + "\n")
        f.flush()
PY
  WS_PID=$!
  sleep 0.5
else
  echo "  (pip install websocket-client to also check the dashboard feed)"
fi

echo "== smoke: every section 11 route matches its contract"
if python3 "$ROOT/scripts/smoke_api.py" "$BASE" >"$TMP/smoke.log" 2>&1; then
  ok "smoke_api.py"
else
  bad "smoke_api.py"; sed 's/^/        /' "$TMP/smoke.log"
fi

post() { curl -sf -X POST "$BASE$1" -H 'Content-Type: application/json' -d "$2"; }

echo "== demo beats"
for scenario in wake_ladder email_drop leave_reminder; do
  if out="$(post /api/demo/trigger "{\"user_id\":\"u_demo\",\"scenario\":\"$scenario\"}")"; then
    ok "trigger $scenario: $(python3 -c 'import json,sys; print(json.load(sys.stdin).get("message",""))' <<<"$out")"
  else
    bad "trigger $scenario"
  fi
done

if post /api/demo/trip '{"trip":"bus_trip_01","speed":100}' >/dev/null; then
  ok "trip replay started"
else
  echo "  SKIP  trip replay (no backend/seed/trips/bus_trip_01.json yet, S3)"
fi

if out="$(post /api/sim/load '{"users":100000,"threads":8}')"; then
  ok "load sim: $(python3 -c 'import json,sys; d=json.load(sys.stdin); print(d.get("users"), "users in", d.get("runtime_ms"), "ms")' <<<"$out")"
else
  bad "load sim"
fi

curl -sf "$BASE/api/users/u_demo/plan" >/dev/null && ok "plan" || bad "plan"
curl -sf "$BASE/api/users/u_demo/punctuality" >/dev/null && ok "punctuality" || bad "punctuality"
curl -sf "$BASE/api/users/u_demo/consequences?delay=10" >/dev/null && ok "consequences" || bad "consequences"

if [[ -n "${WS_PID:-}" ]]; then
  sleep 2
  echo "== dashboard feed"
  for type in decision plan call_state sim_result; do
    if grep -q "\"type\" *: *\"$type\"" "$WS_OUT" 2>/dev/null; then ok "ws saw $type"; else bad "ws never saw $type"; fi
  done
fi

if grep -qiE 'segmentation|terminate|abort' "$LOG"; then bad "backend log shows a crash"; fi
kill -0 "$PID" 2>/dev/null && ok "backend still up" || bad "backend died"

echo "== $pass passed, $fail failed"
[[ $fail -eq 0 ]] || { echo "backend log: "; tail -40 "$LOG"; exit 1; }
