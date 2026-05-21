#!/usr/bin/env bash
# Static guards for optional C# scripting (Mono).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

grep -q 'option(USE_CSHARP' "$PROJECT_ROOT/CMakeLists.txt" || fail "USE_CSHARP CMake option missing"
grep -q 'csharp_debug.c' "$PROJECT_ROOT/src/qcommon/csharp_debug.c" 2>/dev/null || \
	[ -f "$PROJECT_ROOT/src/qcommon/csharp_debug.c" ] || fail "csharp_debug.c missing"
grep -q 'Cmd_CsReload_f' "$PROJECT_ROOT/src/qcommon/cmd.c" || fail "cs_reload not registered in cmd.c"
grep -q 'Com_ScriptEmitEvent' "$PROJECT_ROOT/src/qcommon/script_emit.c" || fail "script_emit bridge missing"
grep -q 'CsDebug_Frame' "$PROJECT_ROOT/src/qcommon/common.c" || fail "CsDebug_Frame not called from Com_Frame"
[ -f "$PROJECT_ROOT/src/qcommon/csharp/IdTech3.Engine.cs" ] || fail "IdTech3.Engine.cs missing"
[ -f "$PROJECT_ROOT/docs/CSHARP.md" ] || fail "docs/CSHARP.md missing"

echo "PASS: C# scripting scaffolding"
