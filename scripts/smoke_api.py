#!/usr/bin/env python3
"""Hit every PRD section 11 route on a running backend and validate each JSON response
against its contract schema. Usage: python3 scripts/smoke_api.py [base_url]

Needs `pip install jsonschema`. Exit code 1 if any route fails.
"""
import json
import sys
import urllib.error
import urllib.request
from pathlib import Path

from jsonschema import Draft202012Validator
from referencing import Registry, Resource

BASE = (sys.argv[1] if len(sys.argv) > 1 else "http://localhost:8080").rstrip("/")
ROOT = Path(__file__).resolve().parent.parent / "contracts"
SCHEMAS = {p.name[: -len(".schema.json")]: json.loads(p.read_text()) for p in (ROOT / "schemas").glob("*.schema.json")}
REGISTRY = Registry().with_resources((s["$id"], Resource.from_contents(s)) for s in SCHEMAS.values())


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *args, **kwargs):
        return None


OPENER = urllib.request.build_opener(NoRedirect)


def call(method, path, body=None):
    data = json.dumps(body).encode() if body is not None else (b"" if method == "POST" else None)
    req = urllib.request.Request(BASE + path, data=data, method=method, headers={"Content-Type": "application/json"})
    try:
        with OPENER.open(req, timeout=10) as r:
            return r.status, r.headers, r.read()
    except urllib.error.HTTPError as e:
        return e.code, e.headers, e.read()


failures = 0


def check(label, method, path, schema=None, body=None, status=200, content_type=None):
    global failures
    code, headers, raw = call(method, path, body)
    problems = []
    if code != status:
        problems.append(f"status {code}, expected {status}")
    if content_type and content_type not in headers.get("Content-Type", ""):
        problems.append(f"content-type {headers.get('Content-Type')}")
    if schema and code == status:
        try:
            doc = json.loads(raw)
            errors = list(Draft202012Validator(SCHEMAS[schema], registry=REGISTRY).iter_errors(doc))
            problems += [f"{'/'.join(map(str, e.absolute_path))}: {e.message}" for e in errors[:3]]
        except json.JSONDecodeError:
            problems.append("body is not JSON")
    print(("FAIL " if problems else "ok   ") + f"{method:4} {path:42} {label}" + ("" if not problems else "  " + "; ".join(problems)))
    failures += bool(problems)
    return code, headers, raw


_, _, raw = check("health", "GET", "/health", "health")
# Demo triggers place calls and texts. Never press them on a server that can reach real phones.
REAL_VOICE = json.loads(raw).get("providers", {}).get("voice") not in (None, "sim")
if REAL_VOICE:
    print("      voice provider is real: skipping demo triggers (they would call real phones)")
check("onboarding session", "POST", "/api/onboarding/session", "onboarding_session")
req = json.loads((ROOT / "examples/onboarding_complete_request.json").read_text())
check("onboarding complete", "POST", "/api/onboarding/complete", "onboarding_complete", body=req)
check("contact confirm", "POST", "/api/contacts/c_mom/confirm", "contact_confirm")
_, start_headers, _ = check("oauth start -> redirect", "GET", "/oauth/google/start?user_id=u_demo", status=302)
# The callback only accepts the random, single-use state that start issued, so follow its redirect.
location = start_headers.get("Location", "")
callback = location[location.find("/oauth/google/callback"):] if "/oauth/google/callback" in location else ""
check("oauth callback -> /done", "GET", callback or "/oauth/google/callback?code=fake&state=missing", status=302)
check("oauth callback reused state", "GET", callback or "/oauth/google/callback?code=fake&state=missing", status=400)
check("plan", "GET", "/api/users/u_demo/plan?date=2026-10-26", "plan")
check("consequences", "GET", "/api/users/u_demo/consequences?delay=11", "consequences")
check("punctuality", "GET", "/api/users/u_demo/punctuality", "punctuality")
check("location", "POST", "/api/location", "location", body=json.loads((ROOT / "examples/location_request.json").read_text()))
for scenario in ([] if REAL_VOICE else ["wake_ladder", "leave_reminder", "email_drop"]):
    check(f"demo trigger {scenario}", "POST", "/api/demo/trigger", "demo_trigger", body={"user_id": "u_demo", "scenario": scenario})
check("demo trigger bad scenario", "POST", "/api/demo/trigger", "error", body={"user_id": "u_demo", "scenario": "nope"}, status=400)
trip_status = 200 if (ROOT.parent / "backend/seed/trips/bus_trip_01.json").exists() else 404
check("demo trip", "POST", "/api/demo/trip", "demo_trip" if trip_status == 200 else "error", body={"trip": "bus_trip_01", "speed": 50}, status=trip_status)
check("sim load", "POST", "/api/sim/load", "sim_result", body={"users": 1000, "threads": 2})
for path in ["/twilio/voice", "/twilio/gather", "/twilio/sms"]:
    check("twiml", "POST", path, content_type="text/xml")
check("twilio status", "POST", "/twilio/status", status=204)
check("audio miss -> 404", "GET", "/audio/nope.mp3", "error", status=404)

print(f"\n{'all routes ok' if not failures else f'{failures} route(s) failed'}")
sys.exit(1 if failures else 0)
