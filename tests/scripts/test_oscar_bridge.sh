#!/usr/bin/env bash
# OSCAR source guard: direct raw OSCAR plus optional gateway bridge.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

test -f engine/core/net_oscar.c || fail "missing net_oscar.c"
test -f engine/core/net_oscar_protocol.c || fail "missing net_oscar_protocol.c"
test -f engine/core/net_oscar_raw.c || fail "missing raw OSCAR codec"
test -f tests/unit/test_oscar_protocol.c || fail "missing OSCAR protocol unit test"
test -f tests/unit/test_oscar_raw.c || fail "missing raw OSCAR unit test"
test -f docs/OSCAR_INTEGRATION.md || fail "missing OSCAR integration docs"

rg -q 'unit_oscar_protocol' CMakeLists.txt || fail "missing OSCAR protocol unit test registration"
rg -q 'unit_oscar_raw' CMakeLists.txt || fail "missing raw OSCAR unit test registration"
rg -q 'OSCAR_STATE_AUTHENTICATING' engine/core/net_oscar.h || fail "missing OSCAR state machine"
rg -q 'IDTECH3_OSCAR_TOKEN' engine/core/net_oscar.c docs/OSCAR_INTEGRATION.md || fail "missing protected token/env guidance"
rg -q 'IDTECH3_OSCAR_PASSWORD' engine/core/net_oscar.c docs/OSCAR_INTEGRATION.md || fail "missing protected direct password guidance"
rg -q 'GET /engine HTTP/1.1' engine/core/net_oscar.c || fail "missing gateway WebSocket handshake"
rg -q 'OSCAR_RawBuildLoginSignon' engine/core/net_oscar.c engine/core/net_oscar_raw.c || fail "missing raw OSCAR login path"
rg -q 'OSCAR_RawBuildIM' engine/core/net_oscar.c engine/core/net_oscar_raw.c || fail "missing raw OSCAR IM path"
rg -q 'OSCAR_RawBuildPresence' engine/core/net_oscar.c engine/core/net_oscar_raw.c || fail "missing raw OSCAR presence path"
rg -q 'numeric IP' engine/core/net_oscar.c docs/OSCAR_INTEGRATION.md || fail "missing no-DNS gateway guidance"
rg -q 'OSCAR_ProtocolBuildRoomMessage' engine/core/net_oscar_protocol.c || fail "missing room message builder"
rg -q 'OSCAR_ProtocolParseEvent' engine/core/net_oscar_protocol.c || fail "missing gateway event parser"
rg -q 'OSCAR_Frame' engine/core/net_ip.c || fail "missing OSCAR network frame hook"
rg -q 'OSCAR_Init' engine/core/net_ip.c || fail "missing OSCAR network init hook"
rg -q 'oscar_status' runtime/server/sv_ccmds.c || fail "missing dedicated OSCAR status command"
rg -q 'oscar_announce' runtime/server/sv_ccmds.c || fail "missing dedicated OSCAR announce command"
rg -q 'Engine.Oscar|\"Oscar\"' runtime/server/sv_app_crdt.c || fail "missing server Lua Engine.Oscar table"
rg -q 'Direct mode currently supports login' docs/OSCAR_INTEGRATION.md || fail "docs must describe direct OSCAR scope"

if rg -q 'curl .*oscar|oscar_.*curl|system\(.*oscar' runtime/server engine/core; then
	fail "OSCAR bridge must not shell out to curl/system"
fi
if rg -q 'getaddrinfo|DnsQuery|uv_getaddrinfo' engine/core/net_oscar.c; then
	fail "OSCAR bridge must not run blocking DNS resolution on the network frame path"
fi

pass "OSCAR direct and gateway wiring is present"
