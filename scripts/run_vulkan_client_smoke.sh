#!/usr/bin/env bash
set -euo pipefail

# Smoke test: launch a Vulkan client against the local server using OA content.
# This uses a virtual display (Xvfb) to render without a real screen.

CLIENT_BIN="$PWD/release/idtech3.x86_64"
SERVER_BIN="$PWD/release/idtech3.server.x86_64"
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

# Decide which map to load from the demo mod path
MAP_TO_USE="demo"
BASE_MOD_DIR="/home/tim/Desktop/idtech3/mods/demo"
# Use simple demo map instead of oa_dm1 for now
# if [ -f "${BASE_MOD_DIR}/pak0.pk3" ]; then
#   MAP_TO_USE="oa_dm1"
# fi

echo "Starting virtual display and processes..."

if [ -f /tmp/server_smoke.pid ]; then
  kill "$(cat /tmp/server_smoke.pid)" 2>/dev/null || true
fi
if [ -f /tmp/client_smoke.pid ]; then
  kill "$(cat /tmp/client_smoke.pid)" 2>/dev/null || true
fi

# Start server
${SERVER_BIN} +map q3dm6 > "$LOG" 2>&1 &
SERVER_PID=$!
echo "Server PID: $SERVER_PID"
echo "$SERVER_PID" > /tmp/server_smoke.pid

# Start client
${CLIENT_BIN} +connect localhost +set fs_game mymod +set cl_playIntro 0 > "$LOG.client" 2>&1 &
CLIENT_PID=$!
echo "Client PID: $CLIENT_PID"
echo "$CLIENT_PID" > /tmp/client_smoke.pid

echo "Waiting for processes to initialize..."
sleep 5

echo "Server log ($LOG):"
echo "=================="
tail -20 "$LOG"
echo ""
echo "Client log ($LOG.client):"
echo "========================="
tail -20 "$LOG.client"

echo ""
echo "Monitoring server log (press Ctrl+C to stop)..."
tail -f "$LOG"

