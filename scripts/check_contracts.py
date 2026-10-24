#!/usr/bin/env python3
"""Validate every contracts/examples/<name>.json against contracts/schemas/<name>.schema.json.

Run: make contracts   (needs `pip install jsonschema`)
"""
import json
import sys
from pathlib import Path

from jsonschema import Draft202012Validator
from referencing import Registry, Resource

root = Path(__file__).resolve().parent.parent / "contracts"
schemas = {p.name[: -len(".schema.json")]: json.loads(p.read_text()) for p in (root / "schemas").glob("*.schema.json")}
registry = Registry().with_resources((s["$id"], Resource.from_contents(s)) for s in schemas.values())

failed = 0
examples = sorted((root / "examples").glob("*.json"))
for ex in examples:
    name = ex.stem
    if name not in schemas:
        print(f"MISSING SCHEMA  {ex.name}")
        failed += 1
        continue
    errors = list(Draft202012Validator(schemas[name], registry=registry).iter_errors(json.loads(ex.read_text())))
    for e in errors:
        print(f"FAIL  {ex.name}  {'/'.join(map(str, e.absolute_path))}: {e.message}")
    failed += bool(errors)
for name in sorted(set(schemas) - {e.stem for e in examples} - {"common"}):
    print(f"MISSING EXAMPLE {name}.json")
    failed += 1

print(f"{len(examples)} examples checked, {failed} problems")
sys.exit(1 if failed else 0)
