#!/usr/bin/env bash
set -euo pipefail

# Smoke test: verify engine startup, asset loading, and rendering in virtual display.
# Tests that the client can initialize, load shaders, and run the main loop.
# Note: Tests core functionality without networking; connection issues are separate.

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

# Vulkan settings for virtual display
export VK_ICD_FILENAMES="$(find /usr/share/vulkan/icd.d -name "*.json" | tr '\n' ':')"
export MESA_VK_DEVICE_SELECT="0"
export VK_LOADER_DEBUG="error"

sleep 1

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
${SERVER_BIN} +devmap q3dm6 > "$LOG" 2>&1 &
SERVER_PID=$!
echo "Server PID: $SERVER_PID"
echo "$SERVER_PID" > /tmp/server_smoke.pid

# Start client (standalone mode to test rendering and window creation)
${CLIENT_BIN} +set fs_game mymod +set cl_playIntro 0 +set cl_renderer opengl +set r_mode 6 +set r_windowed 1 > "$LOG.client" 2>&1 &
CLIENT_PID=$!
echo "Client PID: $CLIENT_PID"
echo "$CLIENT_PID" > /tmp/client_smoke.pid

echo "Waiting for server to initialize..."
sleep 3
echo "Starting client..."
sleep 2

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

