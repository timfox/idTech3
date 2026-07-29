#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

test -f examples/p2p_demo/CMakeLists.txt || fail "missing p2p demo CMake"
test -f examples/p2p_demo/p2p_demo.cpp || fail "missing p2p demo source"
test -x examples/p2p_demo/build.sh || fail "missing executable p2p demo build script"
grep -q 'RunSelfTest' examples/p2p_demo/p2p_demo.cpp || fail "missing self-test proof"
grep -q 'Start Host' examples/p2p_demo/p2p_demo.cpp || fail "missing host button"
grep -q 'Connect Client' examples/p2p_demo/p2p_demo.cpp || fail "missing client button"
grep -q 'Send Ping' examples/p2p_demo/p2p_demo.cpp || fail "missing ping button"

"$ROOT/examples/p2p_demo/build.sh" >/tmp/idtech3_p2p_demo_build.log
"$ROOT/build/examples/p2p_demo/p2p_demo" --self-test >/tmp/idtech3_p2p_demo_run.log
grep -q 'PROOF: ping -> pong succeeded' /tmp/idtech3_p2p_demo_run.log || {
	cat /tmp/idtech3_p2p_demo_run.log >&2
	fail "p2p demo proof did not succeed"
}

pass "p2p demo builds and proves ping/pong"

