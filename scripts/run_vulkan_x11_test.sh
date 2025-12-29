#!/usr/bin/env bash
set -euo pipefail

#
# Run a Vulkan render test forcing X11, using possible OA assets if present,
# and capture metrics logs for triage.
#

LOG_FILE="run_vulkan_x11.log"
rm -f "$LOG_FILE"

echo "Starting Vulkan test with X11 fallback..."

# Prefer explicit OA-like assets if present
RELEASE_BASE_DIR="${PROJECT_ROOT:-/home/tim/Desktop/idtech3}/release/base"
if [ -d "$RELEASE_BASE_DIR" ]; then
  echo "Using release base-dir: $RELEASE_BASE_DIR" | tee -a "$LOG_FILE"
else
  echo "Release base-dir not found at ${RELEASE_BASE_DIR}; continuing without OA assets." | tee -a "$LOG_FILE"
fi

echo "Forcing X11 video driver and running Vulkan..." | tee -a "$LOG_FILE"
export SDL_VIDEODRIVER=x11
VK_METRICS_ENABLED=1 VK_VERBOSE_PIPELINE_LOGS=1 \
./release/idtech3.x86_64 +set fs_game mymod +set cl_renderer vulkan \
  > "$LOG_FILE" 2>&1

echo "Test complete. Logs written to $LOG_FILE" | tee -a "$LOG_FILE"
grep -E "VK_CreateSurface|renderpass_alloc|pipeline_alloc|REPORT" "$LOG_FILE" || true

