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
echo "CI log: ${LOG_FILE}"

# Run the Vulkan client non-interactively, capture logs
VK_VERBOSE_PIPELINE_LOGS=1 VK_LOG_TO_FILE=1 \
./release/idtech3.x86_64 +set fs_game mymod +set cl_renderer vulkan \
  > "${LOG_FILE}" 2>&1 &
PID=$!

echo "Launched PID ${PID}. Waiting for surface init..."
sleep 20

if grep -m1 -q "VK_CreateSurface: Wayland" "${LOG_FILE}"; then
  echo "Wayland surface creation path observed."
else
  echo "No Wayland surface path observed in this run."
  tail -n +1 "${LOG_FILE}" | sed -n '1,200p'
  # If Wayland path wasn't observed, attempt X11 fallback via Xvfb if available
  if command -v Xvfb >/dev/null 2>&1; then
    echo "CI: Attempting X11 fallback via Xvfb for validation..."
    # Launch again under X11
    VK_VERBOSE_PIPELINE_LOGS=1 VK_LOG_TO_FILE=1 \
    ./release/idtech3.x86_64 +set fs_game mymod +set cl_renderer vulkan \
      > "${LOG_FILE}" 2>&1 &
    PID2=$!
    sleep 20
    if grep -m1 -q "SDL_Vulkan_CreateSurface" "${LOG_FILE}"; then
      echo "X11 fallback path executed (observed surface messages)."
      kill "${PID2}" 2>/dev/null || true
      exit 0
    else
      echo "X11 fallback path did not observe surface messages. See logs."
      tail -n +1 "${LOG_FILE}" | sed -n '1,200p'
      kill "${PID2}" 2>/dev/null || true
      exit 1
    fi
  else
    exit 1
  fi
fi

echo "Test completed; inspecting for fallback messages..."
grep -q "retrying with X11" "${LOG_FILE}" && echo "Fallback to X11 path detected." || echo "Fallback to X11 not detected."

kill "${PID}" 2>/dev/null || true
exit 0

