#!/usr/bin/env bash
# Validate all Petri net JSON models under examples/demo_game/beta_traces.
set -euo pipefail

ROOT="${1:-$(cd "$(dirname "$0")/../.." && pwd)}"
TRACE_DIR="${ROOT}/examples/demo_game/beta_traces"

fail() {
	echo "test_beta_petri_validate: $*" >&2
	exit 1
}

[[ -d "${TRACE_DIR}" ]] || fail "missing ${TRACE_DIR}"

shopt -s nullglob
files=( "${TRACE_DIR}"/*.petrinet.json )
[[ ${#files[@]} -gt 0 ]] || fail "no .petrinet.json files under ${TRACE_DIR}"

for f in "${files[@]}"; do
	python3 - "$f" <<'PY'
import json, sys
path = sys.argv[1]
with open(path, encoding="utf-8") as fh:
    doc = json.load(fh)
if doc.get("version") != 1:
    raise SystemExit(f"{path}: version must be 1")
places = doc.get("places")
transitions = doc.get("transitions")
if not isinstance(places, list) or not places:
    raise SystemExit(f"{path}: places[] required")
if not isinstance(transitions, list) or not transitions:
    raise SystemExit(f"{path}: transitions[] required")
place_ids = set()
for p in places:
    pid = p.get("id")
    if not pid:
        raise SystemExit(f"{path}: place missing id")
    place_ids.add(pid)
initial = doc.get("initial", [])
if initial:
    for pid in initial:
        if pid not in place_ids:
            raise SystemExit(f"{path}: initial place {pid} not in places")
for t in transitions:
    if not t.get("id"):
        raise SystemExit(f"{path}: transition missing id")
    msg = t.get("message") or {}
    if not msg.get("type"):
        raise SystemExit(f"{path}: transition {t.get('id')} missing message.type")
    for arc in t.get("input", []):
        if arc not in place_ids:
            raise SystemExit(f"{path}: input {arc} unknown")
    for arc in t.get("output", []):
        if arc not in place_ids:
            raise SystemExit(f"{path}: output {arc} unknown")
print(f"ok: {path}")
PY
done

echo "test_beta_petri_validate: ${#files[@]} model(s) passed"
