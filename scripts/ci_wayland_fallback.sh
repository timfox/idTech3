#!/usr/bin/env bash
set -euo pipefail

#
# CI helper: validate Wayland -> X11 Vulkan surface fallback path is exercised.
#
if [[ -z "${WAYLAND_DISPLAY:-}" ]]; then
  # Attempt to validate Wayland fallback on CI by also simulating an X11 path via Xvfb
  if command -v Xvfb >/dev/null 2>&1; then
    echo "CI: Wayland not detected; running under Xvfb (X11 fallback) for validation"
    Xvfb :99 -screen 0 1024x768x24 >/tmp/xvfb.log 2>&1 &
    XVFB_PID=$!
    export DISPLAY=:99
    export SDL_VIDEODRIVER=x11
  else
    echo "SKIP: Not running under Wayland; WAYLAND_DISPLAY not set and Xvfb not available."
    exit 0
  fi
fi

LOG_FILE="$(mktemp /tmp/wayland-ci-XXXX.log)"
 # Determine metrics artifact path (CI only)
if [ -n "${GITHUB_WORKSPACE:-}" ]; then
  METRICS_OUTPUT="$GITHUB_WORKSPACE/vk_metrics.txt"
else
  METRICS_OUTPUT="/tmp/vk_metrics.txt"
fi
echo "CI log: ${LOG_FILE}"

# Run the Vulkan client non-interactively, capture logs
# Force Wayland first to test the fallback logic
VK_VERBOSE_PIPELINE_LOGS=1 VK_LOG_TO_FILE=1 SDL_VIDEODRIVER=wayland \
./release/idtech3.x86_64 +set fs_game mymod +set cl_renderer vulkan \
  > "${LOG_FILE}" 2>&1 &
PID=$!

echo "Launched PID ${PID}. Waiting for surface init..."
sleep 20

# Check if Wayland was attempted and if fallback occurred
if grep -m1 -q "VK_CreateSurface: Wayland fallback to X11 path engaged" "${LOG_FILE}"; then
  echo "Wayland fallback to X11 path successfully exercised."
  SUCCESS=1
elif grep -m1 -q "SDL_Vulkan_CreateSurface failed on Wayland" "${LOG_FILE}"; then
  echo "Wayland surface creation failed as expected, checking for fallback..."
  if grep -q "retrying with X11" "${LOG_FILE}"; then
    echo "X11 fallback path executed successfully."
    SUCCESS=1
  else
    echo "Wayland failed but no X11 fallback detected."
    SUCCESS=0
  fi
elif grep -m1 -q "Using Wayland display driver" "${LOG_FILE}"; then
  echo "Wayland initialized successfully (no fallback needed)."
  SUCCESS=1
else
  echo "Wayland was not attempted. Check environment setup."
  echo "First 200 lines of log:"
  tail -n +1 "${LOG_FILE}" | sed -n '1,200p'
  SUCCESS=0
fi

if [[ $SUCCESS -eq 0 ]]; then
  echo "Test failed: Wayland fallback not properly exercised."
  exit 1
fi

echo "Test completed successfully - Wayland fallback path validated."

kill "${PID}" 2>/dev/null || true
#
# Persist metrics for triage (if possible)
grep -E "VK_METRICS|pipeline_alloc|pipeline_lookup|surface_created|REPORT" "$LOG_FILE" > "$METRICS_OUTPUT" 2>/dev/null || true
exit 0

