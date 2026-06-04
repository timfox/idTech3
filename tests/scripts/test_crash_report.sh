#!/usr/bin/env bash
# CI: crash reporting symbols and cvars present (no fatal trigger).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

grep -q 'com_crashReportEnable' src/qcommon/com_crash.c
grep -q 'com_crashReportURL' src/qcommon/com_crash.c
grep -q 'Com_Crash_OnFatal' src/qcommon/com_crash.c
test -f docs/CRASH_REPORTING.md
echo "test_crash_report: OK"
