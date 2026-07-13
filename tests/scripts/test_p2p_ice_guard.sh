#!/usr/bin/env bash
# P2P ICE peer-ownership guard: static coverage plus optional live spoof test.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

test -f engine/core/net_p2p_ice.c || fail "missing net_p2p_ice.c"
grep -q 'ignoring candidates from unexpected peer' engine/core/net_p2p_ice.c || fail "missing unexpected peer candidate guard"
grep -q 'ignoring check ack from unexpected peer' engine/core/net_p2p_ice.c || fail "missing unexpected peer check-ack guard"
grep -q 'NET_P2P_PrintPathStatus' runtime/server/sv_ccmds.c || fail "missing path status hook in server p2p_status"
grep -q 'NET_P2P_PrintPathStatus' runtime/client/core/cl_cmds.c || fail "missing path status hook in client p2p_status"

if [ "${IDTECH3_P2P_ICE_GUARD_LIVE:-0}" != "1" ]; then
	pass "test_p2p_ice_guard: static checks ok (set IDTECH3_P2P_ICE_GUARD_LIVE=1 for live)"
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
[ -n "${SERVER_BIN:-}" ] || fail "missing idtech3_server for live ICE guard test"

python3 - "$SERVER_BIN" <<'PY' || fail "live ICE guard flow failed"
import os
import signal
import socket
import subprocess
import sys
import tempfile
import time

server_bin = sys.argv[1]
target_port = int(os.environ.get("P2P_ICE_GUARD_TARGET_PORT", "27972"))
listen_port = int(os.environ.get("P2P_ICE_GUARD_LISTEN_PORT", "27973"))
timeout_ms = int(os.environ.get("P2P_ICE_GUARD_TIMEOUT_MS", "5000"))
game_base = os.environ.get("P2P_ICE_GUARD_GAME_BASE", "")
game_dir = os.environ.get("P2P_ICE_GUARD_GAME_DIR", "base")
map_name = os.environ.get("P2P_ICE_GUARD_MAP", "q3dm1")

log_file = tempfile.NamedTemporaryFile(prefix="p2p-ice-guard-", suffix=".log", delete=False)
log_path = log_file.name
log_file.close()

cmd = [
    server_bin,
    "+set", "dedicated", "1",
    "+set", "net_port", str(listen_port),
    "+set", "net_p2p", "1",
    "+set", "net_p2pBackend", "direct_udp",
    "+set", "net_p2pIceTimeout", str(timeout_ms),
    "+set", "com_hunkMegs", "64",
]

if game_base:
    cmd.extend([
        "+set", "fs_basepath", game_base,
        "+set", "fs_game", game_dir,
        "+map", map_name,
    ])

proc = None
log_handle = None
try:
    log_handle = open(log_path, "w", buffering=1)
    proc = subprocess.Popen(
        cmd,
        stdin=subprocess.PIPE,
        stdout=log_handle,
        stderr=subprocess.STDOUT,
        text=True,
    )

    time.sleep(2.0)
    if proc.poll() is not None:
        with open(log_path, "r", encoding="utf-8", errors="replace") as fh:
            early_log = fh.read()
        if "No game data" in early_log and not game_base:
            print(f"live ICE guard skipped: no game data ({log_path})")
            raise SystemExit(0)
        raise SystemExit(f"server exited early, see {log_path}")

    proc.stdin.write(f"p2p_connect udp:127.0.0.1:{target_port}\n")
    proc.stdin.flush()
    time.sleep(1.0)

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        def oob(msg: str) -> bytes:
            return b"\xff\xff\xff\xff" + msg.encode("ascii")

        sock.sendto(oob("p2pCand host=udp:127.0.0.1:27999"), ("127.0.0.1", listen_port))
        sock.sendto(oob("p2pCheckAck 12345"), ("127.0.0.1", listen_port))
    finally:
        sock.close()

    time.sleep(1.0)
    proc.stdin.write("p2p_status\n")
    proc.stdin.write("quit\n")
    proc.stdin.flush()

    try:
        proc.wait(timeout=12.0)
    except subprocess.TimeoutExpired:
        proc.terminate()
        proc.wait(timeout=5.0)

    with open(log_path, "r", encoding="utf-8", errors="replace") as fh:
        log = fh.read()

    if "ignoring candidates from unexpected peer" not in log:
        raise SystemExit(f"missing candidate guard log in {log_path}")
    if "ignoring check ack from unexpected peer" not in log:
        raise SystemExit(f"missing check-ack guard log in {log_path}")
    if "P2P ICE: active peer" not in log and "P2P ICE: last result fallback peer" not in log:
        raise SystemExit(f"missing ICE status output in {log_path}")

    print(f"live ICE guard ok ({log_path})")
finally:
    if proc is not None and proc.poll() is None:
        try:
            proc.terminate()
            proc.wait(timeout=5.0)
        except Exception:
            try:
                proc.kill()
            except Exception:
                pass
    if log_handle is not None:
        log_handle.close()
PY

pass "test_p2p_ice_guard: live ok"
