#!/usr/bin/env bash
# P2P reconnect: static wiring checks; optional live test when game VM is available.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

if [ "${SKIP_P2P_RECONNECT:-0}" = "1" ]; then
	pass "test_p2p_reconnect skipped (SKIP_P2P_RECONNECT=1)"
	exit 0
fi

test -f runtime/client/core/cl_p2p_session.c || fail "missing cl_p2p_session.c"
test -f runtime/server/sv_client.c || fail "missing sv_client.c"
grep -q 'SV_P2P_HandleReconnectRequest' runtime/server/sv_client.c || fail "missing reconnect handler"
grep -q 'SV_P2P_SaveGraceSlot' runtime/server/sv_client.c || fail "missing grace slot saver"
grep -q 'SV_P2P_AllowReconnectGrace' runtime/server/sv_client.c || fail "missing grace allow helper"
grep -q 'p2pReconnect' runtime/server/sv_main.c || fail "missing p2pReconnect OOB dispatch"
grep -q 'CL_P2P_SessionPrepareDisconnect' runtime/client/core/cl_p2p_session.c || fail "missing disconnect prepare"
grep -q 'CL_P2P_SessionOnConnectFromServerInfo' runtime/client/core/cl_p2p_session.c || fail "missing serverinfo session cache"
grep -q 'CL_P2P_SessionFrame' runtime/client/core/cl_frame.c || fail "missing session frame hook"
grep -q 'cl_p2pReconnectMaxAttempts' runtime/client/core/cl_p2p_session.c || fail "missing reconnect attempt cap"
grep -q 'cl_p2pReconnectJitterMs' runtime/client/core/cl_p2p_session.c || fail "missing reconnect jitter control"
grep -q 'p2p_reconnect_stopped' runtime/client/core/cl_p2p_session.c || fail "missing reconnect stop event"
grep -q 'CL_P2P_SessionCurrentTarget' runtime/client/core/cl_p2p_session.c || fail "missing current reconnect target tracking"
grep -q 'CL_P2P_SessionRecoveryStopReason' runtime/client/core/cl_p2p_session.c || fail "missing reconnect stop reason tracking"
grep -q 'currentTarget' runtime/game/g_lua_bindings.c || fail "missing Lua current reconnect target exposure"
grep -q 'recoveryStopReason' runtime/game/g_lua_bindings.c || fail "missing Lua reconnect stop reason exposure"
grep -q 'cl_p2pReconnectMaxAttempts' docs/P2P_NETWORKING.md || fail "missing reconnect attempt cap docs"

if [ "${IDTECH3_P2P_RECONNECT_LIVE:-0}" != "1" ]; then
	pass "test_p2p_reconnect: static checks ok (set IDTECH3_P2P_RECONNECT_LIVE=1 for live)"
	exit 0
fi

resolve_bin() {
	local name="$1"
	for p in "$ROOT/release/${name}" "$ROOT/build-vk-Release/${name}"; do
		[ -x "$p" ] && echo "$p" && return 0
	done
	return 1
}

SERVER_BIN="$(resolve_bin idtech3_server || true)"
[ -n "${SERVER_BIN:-}" ] || fail "missing idtech3_server for live reconnect test"

GAME_BASE="${P2P_RECONNECT_GAME_BASE:-}"
[ -n "$GAME_BASE" ] || fail "set P2P_RECONNECT_GAME_BASE to a runnable base/ with qagame"

PORT="${P2P_RECONNECT_PORT:-27964}"
SESSION_ID="${P2P_RECONNECT_SESSION:-p2p-reconnect-test}"
LOG="$(mktemp)"
trap 'kill $(jobs -p) 2>/dev/null || true; rm -f "$LOG"' EXIT

"$SERVER_BIN" +set dedicated 2 +set net_port "$PORT" +set fs_basepath "$GAME_BASE" +set fs_game base \
	+set net_p2p 1 +set sv_p2pSessionId "$SESSION_ID" +set sv_p2pReconnectWindow 45 \
	+set sv_p2pFailover reconnect +set sv_maxclients 8 +set com_hunkMegs 128 \
	+map "${P2P_RECONNECT_MAP:-q3dm1}" >"$LOG" 2>&1 &
SRV_PID=$!
sleep 6

kill -0 "$SRV_PID" 2>/dev/null || { cat "$LOG" >&2; fail "server exited early"; }

python3 - "$PORT" "$SESSION_ID" <<'PY' || { cat "$LOG" >&2; fail "live reconnect flow failed"; }
import socket, sys, random, time, subprocess

port = int(sys.argv[1])
session_id = sys.argv[2]
host = "127.0.0.1"

def oob(msg: str) -> bytes:
    return b"\xff\xff\xff\xff" + msg.encode("ascii")

def info_string(pairs):
    return "".join(f"\\{k}\\{v}" for k, v in pairs)

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("127.0.0.1", 0))
sock.settimeout(5.0)

sock.sendto(oob("getchallenge 1 idTech3"), (host, port))
data, _ = sock.recvfrom(4096)
text = data[4:].decode("latin1", errors="replace")
if "challengeResponse" not in text:
    raise SystemExit(f"no challengeResponse: {text!r}")
server_challenge = int(text.split()[1])

qport = random.randint(1, 0xFFFF)
userinfo = info_string([
    ("name", "p2p_reconnect_bot"),
    ("rate", "25000"),
    ("snaps", "20"),
    ("model", "sarge"),
    ("headmodel", "sarge"),
    ("challenge", str(server_challenge)),
    ("qport", str(qport)),
    ("protocol", "72"),
    ("client", "idtech3-test"),
])
sock.sendto(oob(f'connect "{userinfo}"'), (host, port))
time.sleep(1.5)

subprocess.run(
    ["python3", "-c", f'import os; os.write(1, b"clientkick 0\\n")'],
    input=b"clientkick 0\n",
    stdout=subprocess.PIPE,
    check=False,
)
time.sleep(2)

sock.sendto(oob(f"p2pReconnect {session_id}"), (host, port))
data, _ = sock.recvfrom(4096)
resp = data[4:].decode("latin1", errors="replace")
if "challengeResponse" not in resp:
    raise SystemExit(f"post-kick reconnect failed: {resp!r}")
print("live reconnect ok")
PY

rg -q "P2P reconnect: fast challenge" "$LOG" || fail "server log missing fast reconnect challenge"
kill "$SRV_PID" 2>/dev/null || true
pass "test_p2p_reconnect: live ok"
