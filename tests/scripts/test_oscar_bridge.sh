#!/usr/bin/env bash
# OSCAR source/runtime guard: direct raw OSCAR plus optional gateway bridge + hybrid AIM shell.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }
mode="${1:-all}"

source_checks() {
	test -f engine/core/net_oscar.c || fail "missing net_oscar.c"
	test -f engine/core/net_oscar_protocol.c || fail "missing net_oscar_protocol.c"
	test -f engine/core/net_oscar_raw.c || fail "missing raw OSCAR codec"
	test -f tests/unit/test_oscar_protocol.c || fail "missing OSCAR protocol unit test"
	test -f tests/unit/test_oscar_raw.c || fail "missing raw OSCAR unit test"
	test -f docs/OSCAR_INTEGRATION.md || fail "missing OSCAR integration docs"
	test -f runtime/client/core/cl_oscar.c || fail "missing client OSCAR shell"
	test -f examples/demo_game/mod/demo_oscar_aim.cfg || fail "missing OSCAR AIM demo cfg"
	test -f renderers/vulkan/inspector/vk_imgui_oscar_panels.cpp || fail "missing OSCAR ImGui panel"

	rg -q 'unit_oscar_protocol' CMakeLists.txt || fail "missing OSCAR protocol unit test registration"
	rg -q 'unit_oscar_raw' CMakeLists.txt || fail "missing raw OSCAR unit test registration"
	rg -q 'unit_oscar_roster' CMakeLists.txt || fail "missing OSCAR roster unit test registration"
	test -f tests/unit/test_oscar_roster.c || fail "missing OSCAR roster unit test"
	test -f engine/core/net_oscar_roster.c || fail "missing OSCAR roster module"
	rg -q 'OSCAR_STATE_AUTHENTICATING' engine/core/net_oscar.h || fail "missing OSCAR state machine"
	rg -q 'OSCAR_BuddyCount' engine/core/net_oscar.h engine/core/net_oscar.c || fail "missing buddy roster API"
	rg -q 'OSCAR_GetRosterGeneration' engine/core/net_oscar.h engine/core/net_oscar.c || fail "missing roster generation"
	rg -q 'OSCAR_SetEventSink' engine/core/net_oscar.h runtime/client/core/cl_oscar.c || fail "missing OSCAR client event sink"
	rg -q 'OSCAR_RawBuildChatLeave' engine/core/net_oscar.c engine/core/net_oscar_raw.c || fail "missing chat leave/signoff builder"
	rg -q 'CL_Oscar_EventSink' runtime/client/core/cl_oscar.c || fail "missing client AIM notify sink"
	rg -q 'IDTECH3_OSCAR_TOKEN' engine/core/net_oscar.c docs/OSCAR_INTEGRATION.md || fail "missing protected token/env guidance"
	rg -q 'IDTECH3_OSCAR_PASSWORD' engine/core/net_oscar.c docs/OSCAR_INTEGRATION.md || fail "missing protected direct password guidance"
	rg -q 'GET /engine HTTP/1.1' engine/core/net_oscar.c || fail "missing gateway WebSocket handshake"
	rg -q 'OSCAR_RawBuildLoginSignon' engine/core/net_oscar.c engine/core/net_oscar_raw.c || fail "missing raw OSCAR login path"
	rg -q 'OSCAR_RawBuildIM' engine/core/net_oscar.c engine/core/net_oscar_raw.c || fail "missing raw OSCAR IM path"
	rg -q 'OSCAR_RawBuildPresence' engine/core/net_oscar.c engine/core/net_oscar_raw.c || fail "missing raw OSCAR presence path"
	rg -q 'OSCAR_RawBuildBuddyAddTemp' engine/core/net_oscar.c engine/core/net_oscar_raw.c || fail "missing raw OSCAR buddy subscribe path"
	rg -q 'OSCAR_RawParsePresence' engine/core/net_oscar.c engine/core/net_oscar_raw.c || fail "missing raw OSCAR inbound presence parser"
	rg -q 'OSCAR_RawBuildChatMessage' engine/core/net_oscar.c engine/core/net_oscar_raw.c || fail "missing raw OSCAR chat message builder"
	rg -q 'OSCAR_RawParseServiceReply' engine/core/net_oscar.c engine/core/net_oscar_raw.c || fail "missing raw OSCAR service response parser"
	rg -q 'OSCAR_JoinRoomDirect|chatPhase|chatSocket' engine/core/net_oscar.c || fail "missing direct Chat service path"
	rg -q 'OSCAR_RegisterCommands' engine/core/net_oscar.c || fail "missing shared OSCAR console commands"
	rg -q 'oscar_buddies' engine/core/net_oscar.c || fail "missing oscar_buddies command"
	rg -q 'numeric IP' engine/core/net_oscar.c docs/OSCAR_INTEGRATION.md || fail "missing no-DNS gateway guidance"
	rg -q 'OSCAR_ProtocolBuildRoomMessage' engine/core/net_oscar_protocol.c || fail "missing room message builder"
	rg -q 'OSCAR_ProtocolParseEvent' engine/core/net_oscar_protocol.c || fail "missing gateway event parser"
	rg -q 'OSCAR_Frame' engine/core/net_ip.c || fail "missing OSCAR network frame hook"
	rg -q 'OSCAR_Init' engine/core/net_ip.c || fail "missing OSCAR network init hook"
	rg -q 'CL_Oscar_Init|CL_Oscar_Frame' runtime/client/core/cl_oscar.c runtime/client/core/cl_main.c runtime/client/core/cl_frame.c || fail "missing client OSCAR shell wiring"
	rg -q 'Engine.Oscar|\"Oscar\"' runtime/server/services/sv_app_crdt.c runtime/client/core/cl_oscar.c || fail "missing Lua Engine.Oscar table"
	rg -q 'BuddyCount|GetBuddy' runtime/server/services/sv_app_crdt.c runtime/client/core/cl_oscar.c || fail "missing Lua OSCAR buddy roster bindings"
	rg -q 'VkImgui_DrawOscarPanel' renderers/vulkan/inspector/vk_imgui.cpp renderers/vulkan/inspector/vk_imgui_oscar_panels.cpp || fail "missing OSCAR ImGui draw wiring"
	rg -q 'demo_oscar_poll|demo_oscar_aim.lua' examples/demo_game/mod/demo_oscar_aim.cfg examples/demo_game/mod/scripts/lua/demo_oscar_aim.lua || fail "missing OSCAR Lua demo poll loop"
	rg -q 'Hybrid AIM|hybrid' docs/OSCAR_INTEGRATION.md || fail "docs must describe hybrid AIM model"
	rg -q 'Chat service' docs/OSCAR_INTEGRATION.md || fail "docs must describe direct Chat rooms"
	rg -q 'idtech3_minimal_oscar_smoke' tests/scripts/idtech3_minimal_content_smoke.sh || fail "missing OSCAR runtime smoke helper"

	if rg -q 'curl .*oscar|oscar_.*curl|system\(.*oscar' runtime/server engine/core; then
		fail "OSCAR bridge must not shell out to curl/system"
	fi
	if rg -q 'getaddrinfo|DnsQuery|uv_getaddrinfo' engine/core/net_oscar.c; then
		fail "OSCAR bridge must not run blocking DNS resolution on the network frame path"
	fi

	pass "OSCAR direct and gateway wiring is present"
}

runtime_checks() {
	local build="${BUILD_DIR:-$ROOT/build-vk-Release}"
	if [[ -x "$build/unit_oscar_protocol" ]]; then
		"$build/unit_oscar_protocol"
	elif [[ "${IDTECH3_RUNTIME_REQUIRED:-0}" == "1" ]]; then
		fail "unit_oscar_protocol is not built"
	else
		echo "SKIP: unit_oscar_protocol not built"
	fi

	if [[ -x "$build/unit_oscar_raw" ]]; then
		"$build/unit_oscar_raw"
	elif [[ "${IDTECH3_RUNTIME_REQUIRED:-0}" == "1" ]]; then
		fail "unit_oscar_raw is not built"
	else
		echo "SKIP: unit_oscar_raw not built"
	fi

	if [[ -x "$build/unit_oscar_roster" ]]; then
		"$build/unit_oscar_roster"
	elif [[ "${IDTECH3_RUNTIME_REQUIRED:-0}" == "1" ]]; then
		fail "unit_oscar_roster is not built"
	else
		echo "SKIP: unit_oscar_roster not built"
	fi

	"$ROOT/tests/scripts/idtech3_minimal_content_smoke.sh" oscar
	pass "OSCAR runtime smoke"
}

case "$mode" in
	source) source_checks ;;
	runtime) runtime_checks ;;
	all) source_checks; runtime_checks ;;
	*) echo "usage: $0 [source|runtime|all]" >&2; exit 2 ;;
esac
