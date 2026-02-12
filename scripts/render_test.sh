#!/usr/bin/env bash
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RELEASE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)/release"
ENGINE="$RELEASE_DIR/idtech3"

if [ ! -x "$ENGINE" ]; then
  echo "render_test: engine binary missing at $ENGINE"
  exit 1
fi

TEST_MAP="${1:-q3dm1}"
THRESHOLD="${PERF_THRESHOLD:-30}"
CONFIG="$RELEASE_DIR/render_test.cfg"
LOG="$RELEASE_DIR/render_test.log"
RESULTS="$RELEASE_DIR/render_test_results.json"

cat <<EOF > "$CONFIG"
set fs_game atlas
set com_abnormalexit 1
set com_maxfps 120
set r_fullscreen 0
set r_mode 4
map $TEST_MAP
wait 500
quit
EOF

echo "render_test: running map $TEST_MAP, logging to $LOG"
START=$(date +%s)
"$ENGINE" +exec "$CONFIG" >"$LOG" 2>&1 || true
END=$(date +%s)
DURATION=$((END - START))

FPS=$(grep -oP 'fps:\\s*\\K[0-9.]+' "$LOG" | tail -n1 || echo "0")

python3 <<PY
import json
from datetime import datetime
from pathlib import Path

path = Path("$RESULTS")
path.write_text(json.dumps({
    "test_map": "$TEST_MAP",
    "duration_seconds": $DURATION,
    "fps": "$FPS",
    "threshold": "$THRESHOLD",
    "timestamp": datetime.utcnow().isoformat() + "Z"
}, indent=2))
PY

python3 - <<PY
fps = float("$FPS")
threshold = float("$THRESHOLD")
if fps < threshold:
    raise SystemExit(f"render_test: fps={fps} below threshold {threshold}")
print(f"render_test: fps={fps} meets threshold {threshold}")
PY
