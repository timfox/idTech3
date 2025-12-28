#!/usr/bin/env bash
set -euo pipefail

echo "Starting Wayland->X11 fallback test for Vulkan surface creation..."

if [[ -z "${WAYLAND_DISPLAY:-}" ]]; then
  echo "Not running in a Wayland session (WAYLAND_DISPLAY not set). Skipping test."
  exit 0
fi

TEST_LOG=$(mktemp)
echo "Output redirected to ${TEST_LOG}"

VK_VERBOSE_PIPELINE_LOGS=0 VK_LOG_TO_FILE=1 \
VK_VERBOSE_PIPELINE_LOGS=0 \
./release/idtech3.x86_64 +set fs_game mymod +set cl_renderer vulkan > "${TEST_LOG}" 2>&1 &
PID=$!
echo "Launched PID ${PID}. Waiting for fallback if any..."
sleep 20
kill $PID 2>/dev/null || true

if grep -q "VK_CreateSurface: Wayland fallback to X11 path engaged" "${TEST_LOG}"; then
  echo "Test passed: detected Wayland fallback log."
 RESULT=0
else
  echo "Test inconclusive: fallback log not found. Review the run log for details."
RESULT=1
fi

rm -f "${TEST_LOG}"
exit $RESULT

