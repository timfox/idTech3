#!/usr/bin/env bash
# Validate beta trace example files (no client/GPU required).
set -euo pipefail

ROOT="${1:-$(cd "$(dirname "$0")/../.." && pwd)}"
TRACE_DIR="${ROOT}/examples/demo_game/beta_traces"

fail() {
	echo "test_beta_trace_format: $*" >&2
	exit 1
}

[[ -d "${TRACE_DIR}" ]] || fail "missing ${TRACE_DIR}"

cmd="${TRACE_DIR}/sample_level.betacmd"
evt="${TRACE_DIR}/sample_level.betaevt"
manifest="${TRACE_DIR}/sample_level.betatest"
petri="${TRACE_DIR}/time_space_door.petrinet.json"

[[ -f "${cmd}" ]] || fail "missing sample_level.betacmd"
[[ -f "${evt}" ]] || fail "missing sample_level.betaevt"
[[ -f "${manifest}" ]] || fail "missing sample_level.betatest"
[[ -f "${petri}" ]] || fail "missing time_space_door.petrinet.json"

grep -q '^# idtech3 betacmd v1' "${cmd}" || fail "betacmd missing magic header"

line_count=$(grep -cve '^#' -e '^[[:space:]]*$' "${cmd}" || true)
[[ "${line_count}" -ge 1 ]] || fail "betacmd has no data lines"

while IFS= read -r line; do
	[[ -z "${line}" ]] && continue
	echo "${line}" | grep -qE '^\{.*\}$' || fail "betaevt line is not JSON object: ${line}"
done < "${evt}"

grep -q '^version=1' "${manifest}" || fail "betatest missing version"
grep -q '^success=' "${manifest}" || fail "betatest missing success"
grep -q '^max_time_ms=' "${manifest}" || fail "betatest missing max_time_ms"

python3 - "${petri}" <<'PY'
import json, sys
path = sys.argv[1]
with open(path, encoding="utf-8") as f:
    doc = json.load(f)
for key in ("places", "transitions"):
    if key not in doc or not isinstance(doc[key], list):
        raise SystemExit(f"missing or invalid {key}")
print("ok")
PY

echo "test_beta_trace_format: all checks passed"
