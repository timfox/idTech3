#!/usr/bin/env bash
# CI: crash reporting symbols and cvars present (no fatal trigger).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"
cd "$ROOT"

COM_CRASH="$(idtech3_file engine/core/com_crash.c src/qcommon/com_crash.c)"

grep -q 'com_crashReportEnable' "$COM_CRASH"
grep -q 'com_crashReportURL' "$COM_CRASH"
grep -q 'Com_Crash_OnFatal' "$COM_CRASH"
test -f docs/CRASH_REPORTING.md
echo "test_crash_report: OK"
