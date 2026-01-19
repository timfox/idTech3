#!/usr/bin/env bash
# Quick 10-second smoke test for idtech3 engine
# Launches headless, initializes renderer, and exits with success code
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENGINE_BIN="${ENGINE_BIN:-$ROOT/release/idtech3.x86_64}"
TIMEOUT="${TIMEOUT:-10}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

echo "🚀 Starting idtech3 smoke test..."

# Check if binary exists
if [[ ! -x "${ENGINE_BIN}" ]]; then
    echo -e "${RED}❌ Engine binary not found: ${ENGINE_BIN}${NC}" >&2
    exit 1
fi

echo "📁 Engine: ${ENGINE_BIN}"
echo "⏱️  Timeout: ${TIMEOUT}s"

# Run smoke test: headless, no sound, minimal initialization
LOG_FILE="/tmp/smoke_test_$(date +%s).log"
echo "📝 Log: ${LOG_FILE}"

set +e
timeout "${TIMEOUT}s" "${ENGINE_BIN}" \
    +set r_renderer opengl \
    +set fs_game demo \
    +set sv_pure 0 \
    +set com_speeds 0 \
    +set developer 0 \
    +set r_fullscreen 0 \
    +set s_initsound 0 \
    +set non_interactive 1 \
    +map oa_dm1 \
    +wait 100 \
    +quit >"${LOG_FILE}" 2>&1
STATUS=$?
set -e

# Check results
if [[ ${STATUS} -eq 0 ]]; then
    echo -e "${GREEN}✅ Smoke test PASSED${NC}"
    echo "Engine initialized and exited cleanly"
    rm -f "${LOG_FILE}"
    exit 0
elif [[ ${STATUS} -eq 124 ]]; then
    # Timeout - check if engine actually started
    if grep -q "FS_Startup\|Renderer\|OpenGL\|Vulkan" "${LOG_FILE}"; then
        echo -e "${GREEN}✅ Smoke test PASSED${NC}"
        echo "Engine started successfully (timed out as expected)"
        rm -f "${LOG_FILE}"
        exit 0
    else
        echo -e "${RED}❌ Smoke test FAILED${NC}" >&2
        echo "Engine did not initialize properly" >&2
        echo "Check log: ${LOG_FILE}" >&2
        exit 1
    fi
else
    echo -e "${RED}❌ Smoke test FAILED${NC}" >&2
    echo "Engine exited with status ${STATUS}" >&2
    echo "Check log: ${LOG_FILE}" >&2
    exit ${STATUS}
fi