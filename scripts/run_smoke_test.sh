#!/usr/bin/env bash
set -euo pipefail

BASE_DIR="/home/tim/Desktop/idtech3/base"
EXTERNAL_PAK="${EXTERNAL_PAK_PATH:-}"
MODS_DEMO="/home/tim/Desktop/idtech3/mods/demo"
SERVER="./build/idtech3.server.x86_64"

echo "== Building base pak0.pk3 from demo content (if present) =="
if [ -n "$EXTERNAL_PAK" ]; then
  echo "Using external pak0.pk3: $EXTERNAL_PAK"
  python3 /home/tim/Desktop/idtech3/tools/build_base_pak0.py "$EXTERNAL_PAK"
else
  python3 /home/tim/Desktop/idtech3/tools/build_base_pak0.py
fi

echo "== Starting Vulkan smoke test (headless) =="
LOG="/tmp/server_smoke.log"
$SERVER +map demo > "$LOG" 2>&1 &
SERVER_PID=$!
echo "Server PID: $SERVER_PID"
sleep 2
tail -n +1 -f "$LOG" | sed -n '1,200p'

echo "Test started; logs at $LOG"
