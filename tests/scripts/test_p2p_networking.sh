#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

grep -q 'Cmd_AddCommand( "p2p_status"' runtime/client/core/cl_cmds.c || fail "missing p2p_status command"
grep -q 'Cmd_AddCommand( "p2p_address"' runtime/client/core/cl_cmds.c || fail "missing p2p_address command"
grep -q 'Cmd_AddCommand( "p2p_connect"' runtime/client/core/cl_cmds.c || fail "missing p2p_connect command"
grep -q 'Cmd_AddCommand( "p2p_connect_browser"' runtime/client/core/cl_cmds.c || fail "missing p2p_connect_browser command"
grep -q 'Cmd_AddCommand( "p2p_status"' runtime/server/sv_ccmds.c || fail "missing dedicated p2p_status command"
grep -q 'Cmd_AddCommand( "p2p_address"' runtime/server/sv_ccmds.c || fail "missing dedicated p2p_address command"
grep -q 'NET_P2P_IsEnabled() && server->p2pAddr\\[0\\]' runtime/client/cl_ui.c || grep -q 'NET_P2P_IsEnabled() && server->p2pAddr\[0\]' runtime/client/cl_ui.c || fail "missing UI join preference for p2paddr"
grep -q 'LAN_ServerMatchesP2PIdentity' runtime/client/cl_ui.c || fail "missing UI-side p2p identity matcher"
grep -q 'LAN_CopyKnownServerMetadata' runtime/client/cl_ui.c || fail "missing favorites p2p metadata copy"
grep -q 'CL_ServerMatchesP2PIdentity' runtime/client/core/cl_serverbrowser.c || fail "missing p2p identity matcher"
grep -q 'server->p2pAvailable' runtime/client/cl_ui.c || fail "missing p2p availability export to UI"
grep -q 'Cvar_Get( "net_p2p"' engine/core/net_sdr.c || fail "missing net_p2p cvar alias"
grep -q 'NET_P2P_GetLocalAddressString' engine/core/net_p2p.c || fail "missing p2p wrapper"
grep -q 'SteamShared_Init' engine/core/steam_shared.c || fail "missing shared Steam bootstrap"
grep -q 'p2paddr' runtime/server/sv_main.c || fail "missing advertised p2p info key"
grep -q 'p2p_connect_browser <local|global|favorites> <index>' docs/P2P_NETWORKING.md || fail "missing P2P browser docs"

pass "optional P2P networking wrapper, commands, and docs are present"
