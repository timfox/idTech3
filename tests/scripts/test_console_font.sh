#!/usr/bin/env bash
# Console font: bitmap default; optional FreeType via cl_builtInTtfConsole.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

grep -q 'cl_builtInTtfConsole' runtime/client/cl_scrn.c || fail 'missing cl_builtInTtfConsole cvar'
grep -q 'SCR_ConsoleTtfEnabled' runtime/client/cl_scrn.c || fail 'missing SCR_ConsoleTtfEnabled'
grep -q '"cl_builtInTtfConsole", "0"' runtime/client/cl_scrn.c || fail 'cl_builtInTtfConsole default must be 0'
grep -q 'console text using bitmap charset' runtime/client/cl_scrn.c || fail 'bitmap console log missing'
grep -q 'SCR_ConsoleCharWidth()' runtime/client/cl_console.c || fail 'console draw must use SCR_ConsoleCharWidth'
grep -q 'SCR_ConsoleCharWidth()' runtime/client/cl_sdf_font.c || fail 'SDF console must use SCR_ConsoleCharWidth'
grep -q 'latchedConsoleTtf' runtime/client/cl_console.c || fail 'console reflow on cl_builtInTtfConsole toggle'
grep -q 'cl_builtInTtfConsole 0' config/classic_baseq3.cfg || fail 'classic_baseq3 must pin bitmap console'

pass "console font rendering contract"
