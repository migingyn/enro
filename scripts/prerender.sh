#!/usr/bin/env bash
# Cache scripted TTS lines (backend/config/scripts.json x persona voices) into backend/audio_cache/
# so the demo plays instantly and costs nothing on stage (PRD 4, AudioCache). Placeholders are
# filled with the demo user's values. Without OPENAI_API_KEY this prints what it would render and exits 0.
# Usage: scripts/prerender.sh [persona_id ...]   (default: every persona)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
[[ -f "$ROOT/.env" ]] && set -a && source "$ROOT/.env" && set +a

exec python3 - "$ROOT" "$@" <<'PY'
import json, os, sys, urllib.request
from pathlib import Path

root = Path(sys.argv[1])
only = set(sys.argv[2:])
scripts_path = root / "backend/config/scripts.json"
personas_path = root / "backend/config/personas.json"
out_dir = root / "backend/audio_cache"

if not scripts_path.exists() or not personas_path.exists():
    print("prerender: backend/config/scripts.json or personas.json missing (S3). Nothing to do.")
    sys.exit(0)

# Demo user values from contracts/examples/plan.json (Sam, interview at Qualcomm).
demo = {"name": "Sam", "event": "your interview", "minutes": "10", "contact": "Mom", "time": "9"}

def fill(line):
    try:
        return line.format(**demo)
    except (KeyError, IndexError):
        return None

# Must match AudioCache::keyFor: "a" + FNV-1a 64 of voice|instructions|text, lowercase hex.
def key_for(text, voice, instructions):
    h = 0xcbf29ce484222325
    for b in f"{voice}|{instructions}|{text}".encode():
        h = ((h ^ b) * 0x100000001b3) & 0xFFFFFFFFFFFFFFFF
    return f"a{h:016x}"

lines = json.loads(scripts_path.read_text())["lines"]
personas = [p for p in json.loads(personas_path.read_text())["archetypes"] if not only or p["id"] in only]

key = os.environ.get("OPENAI_API_KEY", "")
model = os.environ.get("OPENAI_TTS_MODEL") or "gpt-4o-mini-tts"
out_dir.mkdir(parents=True, exist_ok=True)
manifest_path = out_dir / "manifest.json"
manifest = json.loads(manifest_path.read_text()) if manifest_path.exists() else {}

todo = []
for p in personas:
    voice = p.get("voice", {}).get("tts_voice", "alloy")
    instructions = p.get("voice", {}).get("instructions", "")
    for line_key, variants in lines.items():
        for raw in variants:
            text = fill(raw)
            if text:
                todo.append((p["id"], line_key, text, voice, instructions, key_for(text, voice, instructions)))

new = [t for t in todo if not (out_dir / f"{t[5]}.mp3").exists()]
print(f"prerender: {len(todo)} lines across {len(personas)} personas, {len(todo) - len(new)} cached, {len(new)} to render")
if not key:
    print("prerender: OPENAI_API_KEY not set, dry run only (fake mode uses browser speech).")
    sys.exit(0)

for i, (pid, line_key, text, voice, instructions, aid) in enumerate(new, 1):
    body = {"model": model, "voice": voice, "input": text, "response_format": "mp3"}
    if instructions:
        body["instructions"] = instructions
    req = urllib.request.Request(
        "https://api.openai.com/v1/audio/speech",
        data=json.dumps(body).encode(),
        headers={"Authorization": f"Bearer {key}", "Content-Type": "application/json"},
    )
    try:
        with urllib.request.urlopen(req, timeout=60) as r:
            (out_dir / f"{aid}.mp3").write_bytes(r.read())
    except Exception as e:
        print(f"  FAIL {pid}/{line_key}: {e}")
        continue
    manifest[aid] = {"persona": pid, "line": line_key, "text": text, "voice": voice}
    print(f"  [{i}/{len(new)}] {pid}/{line_key} -> {aid}.mp3")

manifest_path.write_text(json.dumps(manifest, indent=2))
print(f"prerender: done, manifest at {manifest_path.relative_to(root)}")
PY
