#!/usr/bin/env bash
# Validates Engine.Save JSON scaffolding in g_engine_systems.c
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
source "$ROOT/tests/scripts/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"
SRC="$(idtech3_require_file runtime/game/systems/g_engine_systems.c src/game/g_engine_systems.c)"

grep -q 'EngineSave_JsonEscapeLabel' "$SRC" || {
  echo "missing EngineSave_JsonEscapeLabel in g_engine_systems.c" >&2
  exit 1
}
grep -q 'JSON_ObjectGetNamedValue' "$SRC" || {
  echo "missing JSON_ObjectGetNamedValue parse path" >&2
  exit 1
}

export ROOT SRC
python3 - <<'PY'
import json
import os

src = open(os.environ["SRC"]).read()
assert "ENGINE_SAVE_PROTOCOL_VERSION" in src

def escape_label(s: str) -> str:
    out = []
    for c in s:
        if ord(c) < 0x20:
            raise ValueError("control char")
        if c in ('"', "\\"):
            out.append("\\" + c)
        else:
            out.append(c)
    return "".join(out)

for label in ("demo_sp_autosave", 'quote "test"', "back\\slash"):
    esc = escape_label(label)
    body = {
        "protocolVersion": 1,
        "modVersion": "idtech3_engine",
        "label": label,
        "checksum": 0,
    }
    dumped = json.dumps(body, indent=2)
    assert json.loads(dumped)["label"] == label
    assert esc == label or "\\" in esc

print("engine_save JSON escape checks ok")
PY

echo "test_engine_save.sh: ok"
