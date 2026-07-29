#!/usr/bin/env bash
# P2P reconnect: static wiring checks + auto live smoke against rtest_base when available.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

# shellcheck source=idtech3_minimal_content_smoke.sh
source "$ROOT/tests/scripts/idtech3_minimal_content_smoke.sh"

fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

if [ "${SKIP_P2P_RECONNECT:-0}" = "1" ]; then
	pass "test_p2p_reconnect skipped (SKIP_P2P_RECONNECT=1)"
	exit 0
fi

test -f runtime/client/core/cl_p2p_session.c || fail "missing cl_p2p_session.c"
test -f runtime/server/client/sv_client.c || fail "missing sv_client.c"
test -f runtime/game/scripting/g_lua_control_bindings.inc || fail "missing Lua control bindings include"
grep -q 'SV_P2P_HandleReconnectRequest' runtime/server/client/sv_client.c || fail "missing reconnect handler"
grep -q 'SV_P2P_SaveGraceSlot' runtime/server/client/sv_client.c || fail "missing grace slot saver"
grep -q 'SV_P2P_PrimeGraceSlot' runtime/server/client/sv_client.c || fail "missing grace prime helper"
grep -q 'SV_P2P_AllowReconnectGrace' runtime/server/client/sv_client.c || fail "missing grace allow helper"
grep -q 'p2pReconnect' runtime/server/core/sv_main.c || fail "missing p2pReconnect OOB dispatch"
grep -q 'p2p_grace_prime' runtime/server/core/sv_ccmds.c || fail "missing p2p_grace_prime command"
grep -q 'CL_P2P_SessionPrepareDisconnect' runtime/client/core/cl_p2p_session.c || fail "missing disconnect prepare"
grep -q 'CL_P2P_SessionOnConnectFromServerInfo' runtime/client/core/cl_p2p_session.c || fail "missing serverinfo session cache"
grep -q 'CL_P2P_SessionFrame' runtime/client/core/cl_frame.c || fail "missing session frame hook"
grep -q 'cl_p2pReconnectMaxAttempts' runtime/client/core/cl_p2p_session.c || fail "missing reconnect attempt cap"
grep -q 'cl_p2pReconnectJitterMs' runtime/client/core/cl_p2p_session.c || fail "missing reconnect jitter control"
grep -q 'p2p_reconnect_stopped' runtime/client/core/cl_p2p_session.c || fail "missing reconnect stop event"
grep -q 'CL_P2P_SessionCurrentTarget' runtime/client/core/cl_p2p_session.c || fail "missing current reconnect target tracking"
grep -q 'CL_P2P_SessionRecoveryStopReason' runtime/client/core/cl_p2p_session.c || fail "missing reconnect stop reason tracking"
grep -q 'currentTarget' runtime/game/scripting/g_lua_bindings.c runtime/game/scripting/g_lua_control_bindings.inc || fail "missing Lua current reconnect target exposure"
grep -q 'recoveryStopReason' runtime/game/scripting/g_lua_bindings.c runtime/game/scripting/g_lua_control_bindings.inc || fail "missing Lua reconnect stop reason exposure"
grep -q 'cl_p2pReconnectMaxAttempts' docs/P2P_NETWORKING.md || fail "missing reconnect attempt cap docs"
grep -q 'NET_P2P_ConsumeDeferredConnect' runtime/client/core/cl_p2p_session.c || fail "missing ICE deferred connect consume"
grep -q 'net_p2pIceDeferConnect' engine/core/net_p2p_ice.c || fail "missing ICE defer-connect cvar"

pass "test_p2p_reconnect: static checks ok"

if [ "${SKIP_P2P_RECONNECT_LIVE:-0}" = "1" ]; then
	pass "test_p2p_reconnect: live skipped (SKIP_P2P_RECONNECT_LIVE=1)"
	exit 0
fi

# Live is automatic when the minimal pack + server exist; force with IDTECH3_P2P_RECONNECT_LIVE=1.
FORCE_LIVE="${IDTECH3_P2P_RECONNECT_LIVE:-0}"
SERVER_BIN=""
GAME_BASE=""
GAME_DIR=""

if SERVER_BIN="$(idtech3_minimal_server "$ROOT")"; then
	:
else
	SERVER_BIN=""
fi

if [ -n "${P2P_RECONNECT_GAME_BASE:-}" ]; then
	GAME_BASE="$(cd "$P2P_RECONNECT_GAME_BASE" && pwd)"
	GAME_DIR="${P2P_RECONNECT_GAME_DIR:-base}"
elif PACK="$(idtech3_minimal_require_pack "$ROOT")"; then
	GAME_BASE="$(cd "$(dirname "$PACK")" && pwd)"
	GAME_DIR="$(basename "$PACK")"
fi

if [ -z "$SERVER_BIN" ] || [ -z "$GAME_BASE" ]; then
	if [ "$FORCE_LIVE" = "1" ]; then
		fail "live reconnect requested but server or game pack missing (set P2P_RECONNECT_GAME_BASE or build idtech3_server)"
	fi
	pass "test_p2p_reconnect: live skipped (no server/pack; set IDTECH3_P2P_RECONNECT_LIVE=1 to require)"
	exit 0
fi

PORT="${P2P_RECONNECT_PORT:-27964}"
SESSION_ID="${P2P_RECONNECT_SESSION:-p2p-reconnect-test}"
MAP_NAME="${P2P_RECONNECT_MAP:-rtest_parity}"
LOG="$(mktemp)"
trap 'kill $(jobs -p) 2>/dev/null || true; rm -f "$LOG"' EXIT

python3 - "$SERVER_BIN" "$GAME_BASE" "$GAME_DIR" "$PORT" "$SESSION_ID" "$MAP_NAME" "$LOG" <<'PY' || { cat "$LOG" >&2; fail "live reconnect flow failed"; }
import socket, subprocess, sys, time

server_bin, game_base, game_dir, port_s, session_id, map_name, log_path = sys.argv[1:8]
port = int(port_s)
host = "127.0.0.1"

cmd = [
    server_bin,
    "+set", "dedicated", "1",
    # Force IPv4 on: archived net_enabled 0 from headless smokes breaks StringToAdr.
    "+set", "net_enabled", "1",
    "+set", "net_ip", host,
    "+set", "net_port", str(port),
    "+set", "fs_basepath", game_base,
    "+set", "fs_game", game_dir,
    "+set", "vm_game", "2",
    "+set", "bot_enable", "0",
    "+set", "net_p2p", "1",
    "+set", "net_p2pStun", "0",
    "+set", "net_p2pBackend", "direct_udp",
    "+set", "sv_p2pSessionId", session_id,
    "+set", "sv_p2pReconnectWindow", "45",
    "+set", "sv_p2pFailover", "reconnect",
    "+set", "sv_maxclients", "8",
    "+set", "com_hunkMegs", "128",
    "+map", map_name,
]

log_handle = open(log_path, "w", buffering=1)
proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=log_handle, stderr=subprocess.STDOUT, text=True)
try:
    for _ in range(60):
        time.sleep(0.25)
        with open(log_path, "r", encoding="utf-8", errors="replace") as fh:
            if "InitGame:" in fh.read():
                break
        if proc.poll() is not None:
            raise SystemExit(f"server exited early, see {log_path}")
    else:
        raise SystemExit(f"InitGame not seen, see {log_path}")

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((host, 0))
    sock.settimeout(5.0)
    client_port = sock.getsockname()[1]

    # connect OOB is Huffman-compressed; prime grace for this UDP endpoint instead.
    proc.stdin.write(f"p2p_grace_prime {host}:{client_port}\n")
    proc.stdin.flush()

    deadline = time.time() + 5.0
    while time.time() < deadline:
        with open(log_path, "r", encoding="utf-8", errors="replace") as fh:
            if "P2P reconnect: primed grace" in fh.read():
                break
        time.sleep(0.1)
    else:
        raise SystemExit(f"grace prime missing in {log_path}")

    sock.sendto(b"\xff\xff\xff\xff" + f"p2pReconnect {session_id}".encode("ascii"), (host, port))
    data, _ = sock.recvfrom(4096)
    resp = data[4:].decode("latin1", errors="replace")
    if "challengeResponse" not in resp:
        raise SystemExit(f"post-prime reconnect failed: {resp!r}")

    deadline = time.time() + 5.0
    while time.time() < deadline:
        with open(log_path, "r", encoding="utf-8", errors="replace") as fh:
            if "P2P reconnect: fast challenge" in fh.read():
                break
        time.sleep(0.1)
    else:
        raise SystemExit(f"server log missing fast reconnect challenge ({log_path})")

    print("live reconnect ok")
finally:
    try:
        if proc.poll() is None:
            proc.stdin.write("quit\n")
            proc.stdin.flush()
            proc.wait(timeout=8.0)
    except Exception:
        try:
            proc.kill()
        except Exception:
            pass
    log_handle.close()
PY

pass "test_p2p_reconnect: live ok"
