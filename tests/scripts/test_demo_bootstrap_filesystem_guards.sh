#!/usr/bin/env bash
# Regression guard for the minimal demo_skeleton bootstrap pk3 and filesystem reference rules.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

python3 - "$PROJECT_ROOT" <<'PY'
import pathlib
import re
import sys
import zipfile


def fail(message):
    print(f"FAIL: {message}", file=sys.stderr)
    sys.exit(1)


root = pathlib.Path(sys.argv[1])
pk3_path = root / "examples" / "demo_skeleton" / "base" / "z_minimal_bootstrap.pk3"
files_c_path = root / "src" / "qcommon" / "files.c"

if not pk3_path.is_file():
    fail(f"missing bootstrap pk3: {pk3_path}")
if not files_c_path.is_file():
    fail(f"missing filesystem source: {files_c_path}")

with zipfile.ZipFile(pk3_path) as archive:
    names = sorted(info.filename for info in archive.infolist() if not info.is_dir())
    if names != ["default.cfg"]:
        fail(f"bootstrap pk3 should contain only default.cfg, found: {names}")

    default_cfg = archive.read("default.cfg").decode("utf-8")
    for marker in (
        "set com_skipIdLogo 1",
        "set com_skipIntroLogo 1",
        "set com_introPlayed 1",
    ):
        if marker not in default_cfg:
            fail(f"default.cfg missing bootstrap marker: {marker}")

files_c = files_c_path.read_text(encoding="utf-8")
match = re.search(
    r"static\s+qboolean\s+FS_GeneralRef\s*\(\s*const\s+char\s+\*filename\s*\)\s*\{(?P<body>.*?)\n\}",
    files_c,
    re.DOTALL,
)
if not match:
    fail("could not locate FS_GeneralRef")

body = match.group("body")
if '"cfg"' not in body:
    fail("FS_GeneralRef must keep .cfg files non-general so default.cfg bootstraps stay lightweight")

qagame_exclusion = re.search(
    r"if\s*\(\s*!\s*Q_stricmp\s*\(\s*filename\s*,\s*\"vm/qagame\.qvm\"\s*\)\s*\)\s*(?:\{\s*)?return\s+qfalse\s*;",
    body,
    re.DOTALL,
)
if not qagame_exclusion:
    fail("FS_GeneralRef must exclude vm/qagame.qvm for qvm-only bootstrap packs")

return_qtrue = body.find("return qtrue;")
if return_qtrue == -1 or return_qtrue < qagame_exclusion.end():
    fail("vm/qagame.qvm exclusion must happen before FS_GeneralRef returns qtrue")

print("PASS: test_demo_bootstrap_filesystem_guards")
PY
