#!/usr/bin/env bash
set -euo pipefail

# Smoke test: launch a Vulkan client against the local server using OA content.
# This uses a virtual display (Xvfb) to render without a real screen.

CLIENT_BIN="$PWD/build/idtech3.x86_64"
SERVER_BIN="$PWD/build/idtech3.server.x86_64"
LOG="/tmp/client_smoke.log"
DISPLAY_NUM="${DISPLAY_NUM:-99}"

ensure_cmd() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "Error: required command '$1' not found. Install it and re-run." >&2
    exit 1
  }
}

ensure_cmd "Xvfb"
ensure_cmd "Xvfb" 2>/dev/null || true

echo "Starting virtual display :$DISPLAY_NUM with 1280x720x24"
Xvfb ":${DISPLAY_NUM}" -screen 0 1280x720x24 >/dev/null 2>&1 &
XVFB_PID=$!
export DISPLAY=":$DISPLAY_NUM"
sleep 0.5

echo "Launching server (oa content path)..."
if [ -f /tmp/server_smoke.pid ]; then
  kill "$(cat /tmp/server_smoke.pid)" 2>/dev/null || true
fi
${SERVER_BIN} +set fs_game demo_content +map oa_dm1 > "$LOG" 2>&1 &
SERVER_PID=$!
echo "Server PID: $SERVER_PID"

echo "Launching client..."
"${CLIENT_BIN}" +set fs_game demo_content +map oa_dm1 > "$LOG.client" 2>&1 &
CLIENT_PID=$!
echo "Client PID: $CLIENT_PID"

echo "Logs:"
tail -n +1 -f "$LOG" &
TAIL_PID=$!

echo "Done. Logs at $LOG and $LOG.client"

