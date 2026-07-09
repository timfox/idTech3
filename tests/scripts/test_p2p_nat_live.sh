#!/usr/bin/env bash
# Two-runner P2P NAT live test (host/client roles). See docs/P2P_NAT_TESTING.md.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

ROLE="${1:-}"
ARTIFACT_DIR="${P2P_NAT_ARTIFACT_DIR:-$ROOT/build-vk-Release/p2p-nat-artifact}"
HOST_JSON="$ARTIFACT_DIR/p2p-nat-host.json"
TIMEOUT_SEC="${P2P_NAT_TIMEOUT_SEC:-60}"

fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

if [ "${IDTECH3_P2P_NAT_LIVE:-0}" != "1" ]; then
	echo "test_p2p_nat_live: skipped (set IDTECH3_P2P_NAT_LIVE=1)"
	exit 0
fi

resolve_bin() {
	local name="$1"
	for p in "$ROOT/release/${name}" "$ROOT/build-vk-Release/${name}"; do
		[ -x "$p" ] && echo "$p" && return 0
	done
	fail "missing executable $name"
}

SERVER_BIN="$(resolve_bin idtech3_server)"

case "$ROLE" in
host)
	mkdir -p "$ARTIFACT_DIR"
	PORT="${P2P_NAT_HOST_PORT:-27970}"
	LOG="$(mktemp)"
	trap 'kill $(jobs -p) 2>/dev/null || true; rm -f "$LOG"' EXIT

	"$SERVER_BIN" +set dedicated 1 +set net_port "$PORT" +set net_p2p 1 \
		+set net_p2pBackend direct_udp \
		+set net_p2pAdvertiseAddress "udp:${P2P_NAT_HOST_IP:-127.0.0.1}:${PORT}" \
		+set sv_p2pReconnectWindow 45 +set sv_p2pFailover reconnect \
		+set com_hunkMegs 64 >"$LOG" 2>&1 &
	SRV_PID=$!
	sleep 2

	ADVERTISE="udp:${P2P_NAT_HOST_IP:-127.0.0.1}:${PORT}"
	printf '{"advertise":"%s","punch_port":%s,"session":"p2p-live-test"}\n' "$ADVERTISE" "$PORT" >"$HOST_JSON"
	echo "P2P NAT host artifact: $HOST_JSON"
	cat "$HOST_JSON"

	END=$((SECONDS + TIMEOUT_SEC))
	while [ $SECONDS -lt $END ]; do
		if ! kill -0 "$SRV_PID" 2>/dev/null; then
			fail "host server exited early"
		fi
		sleep 2
	done

	kill "$SRV_PID" 2>/dev/null || true
	pass "host role completed"
	;;
client)
	[ -f "$HOST_JSON" ] || fail "missing host artifact $HOST_JSON"
	ADVERTISE="$(python3 -c "import json; print(json.load(open('$HOST_JSON'))['advertise'])")"
	PORT="$(python3 -c "import json; print(json.load(open('$HOST_JSON'))['punch_port'])")"
	LOG="$(mktemp)"
	trap 'rm -f "$LOG"' EXIT

	{
		sleep 1
		echo "p2p_punch $ADVERTISE"
		for _ in $(seq 1 20); do
			sleep 1
			echo "p2p_punch_status"
		done
		echo "quit"
	} | timeout "$TIMEOUT_SEC" "$SERVER_BIN" +set dedicated 1 +set net_port "$((PORT + 1))" \
		+set net_p2p 1 +set net_p2pBackend direct_udp +set com_hunkMegs 64 >"$LOG" 2>&1 || true

	rg -q "ack:yes|punch acknowledged" "$LOG" || fail "client punch not acknowledged"
	pass "client punch acknowledged for $ADVERTISE"
	;;
*)
	fail "usage: $0 host|client"
	;;
esac
