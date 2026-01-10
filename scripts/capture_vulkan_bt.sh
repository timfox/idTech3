#!/usr/bin/env bash

# Automated helper to rebuild the Vulkan renderer with symbols, run it under gdb,
# and capture a full backtrace on crash. Outputs are saved under logs/.

set -euo pipefail

ROOT="/home/tim/Desktop/idtech3"
BUILD_DIR="${BUILD_DIR:-${ROOT}/build}"
SOLIB_PATH="${ROOT}/release"
EXE="${SOLIB_PATH}/idtech3.x86_64"
LOG_DIR="${LOG_DIR:-${ROOT}/logs}"
GAME="${GAME:-mymod}"
VK_VALIDATION="${VK_VALIDATION:-1}"
VK_MESH_SHADERS="${VK_MESH_SHADERS:-0}"
VK_RAY_TRACING="${VK_RAY_TRACING:-0}"
VK_DEVICE="${VK_DEVICE:-1}"
EXTRA_ARGS=${EXTRA_ARGS:-}

# Optional positional arg 1 overrides GAME
if [ $# -ge 1 ] && [ -n "${1:-}" ]; then
  GAME="$1"
  shift
fi

# Clear screen only if TTY is available
if [ -t 0 ] && [ -t 1 ]; then
    clear
fi

mkdir -p "${LOG_DIR}"

echo "[1/5] Enabling core dumps (current shell only)…"
ulimit -c unlimited

echo "[2/5] Configuring CMake with symbols (RelWithDebInfo)…"
cmake -S "${ROOT}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=RelWithDebInfo

echo "[3/5] Building targets: idtech3_vulkan_x86_64 and idtech3.x86_64…"
# Build Vulkan renderer
if ! cmake --build "${BUILD_DIR}" --target idtech3_vulkan_x86_64; then
    echo "Warning: Failed to build idtech3_vulkan_x86_64, continuing anyway..."
fi
# Build main executable
if ! cmake --build "${BUILD_DIR}" --target idtech3.x86_64; then
    echo "Error: Failed to build idtech3.x86_64"
    exit 1
fi

timestamp="$(date +%Y%m%d_%H%M%S)"
BT_LOG="${LOG_DIR}/gdb_bt_${timestamp}_${GAME}.log"
GDB_CMDS="${LOG_DIR}/gdb_cmds_${timestamp}_${GAME}.gdb"
GDB_CMDS_OVERRIDE="${GDB_CMDS_OVERRIDE:-}"

if [ -z "${GDB_CMDS_OVERRIDE}" ]; then
  cat > "${GDB_CMDS}" <<'EOF'
set pagination off
set confirm off
set breakpoint pending on
set solib-search-path __SOLIB__
# Catch signals to stop before process exits
handle SIGSEGV stop print
handle SIGABRT stop print
handle SIGFPE stop print
handle SIGILL stop print
handle SIGBUS stop print
# Set breakpoint on fatal error function and auto-capture when hit
# Use Com_Error instead of ri.Error (ri.Error is a function pointer)
break Com_Error
commands
  printf "\n== Fatal error breakpoint hit (Com_Error) ==\n"
  printf "PC=%p\n", $pc
  info symbol $pc
  printf "\n== Full backtrace ==\n"
  thread apply all bt full
  printf "\n== Register state ==\n"
  info registers
  printf "\n== Arguments ==\n"
  info args
  printf "\n== Local variables ==\n"
  info locals
  # Continue to let process exit normally after capturing context
  continue
end
# Also catch abort() calls which may be used for fatal errors
# Note: abort may be in libc, so we use a pending breakpoint
break abort
commands
  printf "\n== Abort called (fatal error) ==\n"
  printf "PC=%p\n", $pc
  info symbol $pc
  printf "\n== Full backtrace ==\n"
  thread apply all bt full
  printf "\n== Register state ==\n"
  info registers
  continue
end
run
# After run completes, the breakpoint commands above will have captured
# context if Com_Error was called. If process exited normally, we can't
# access registers anymore, so just show what we can.
printf "\n== Post-run information ==\n"
# Always show these (they work even after exit)
printf "\n== Shared library info ==\n"
info sharedlibrary idtech3_vulkan_x86_64.so
info files
maintenance info sections
quit
EOF
  sed -i "s|__SOLIB__|${SOLIB_PATH}|g" "${GDB_CMDS}"
else
  GDB_CMDS="${GDB_CMDS_OVERRIDE}"
fi

echo "[4/5] Running under gdb using ${GDB_CMDS} (logs: ${BT_LOG})…"
# Verify executable exists
if [ ! -f "${EXE}" ]; then
    echo "Error: Executable not found: ${EXE}"
    echo "Please build the project first or check BUILD_DIR and SOLIB_PATH settings"
    exit 1
fi

# Verify Vulkan renderer exists
VK_RENDERER="${SOLIB_PATH}/idtech3_vulkan_x86_64.so"
if [ ! -f "${VK_RENDERER}" ]; then
    echo "Warning: Vulkan renderer not found: ${VK_RENDERER}"
    echo "The engine may fall back to OpenGL renderer"
    echo "Continuing anyway..."
fi

# Check if gdb is available
if ! command -v gdb >/dev/null 2>&1; then
    echo "Error: gdb not found. Please install gdb:"
    echo "  sudo apt-get install gdb  # Ubuntu/Debian"
    exit 1
fi

# Set default timeout (30 seconds for testing, can be overridden with GDB_TIMEOUT env var)
# This prevents the game from running indefinitely and creating huge log files
GDB_TIMEOUT="${GDB_TIMEOUT:-30}"
echo "Running game with ${GDB_TIMEOUT} second timeout to capture startup/crash..."
echo "Note: Game will be terminated after ${GDB_TIMEOUT} seconds to prevent log bloat"
echo "Set GDB_TIMEOUT environment variable to change this (e.g., GDB_TIMEOUT=300 for 5 minutes)"

# Run gdb and capture output to file directly to avoid stderr blocking issues
# Use timeout to prevent hanging if the game doesn't exit
# Log size is limited to 50MB to prevent huge files
GDB_EXIT_CODE=0

# Use timeout to prevent hanging, and limit log size with head/tail
if command -v timeout >/dev/null 2>&1; then
    # Use timeout if available, and limit log output to prevent huge files (50MB max)
    timeout "${GDB_TIMEOUT}" gdb --batch -x "${GDB_CMDS}" --args "${EXE}" \
      +set fs_game "${GAME}" \
      +set com_error "1" \
      +set com_developer "1" \
      +set r_developer "1" \
      +set r_fullscreen "0" \
      +set r_mode "6" \
      +set r_windowed "1" \
      +set r_width "800" \
      +set r_height "600" \
      +set r_xpos "100" \
      +set r_ypos "100" \
      +set r_showFPS "1" \
      +set cl_renderer "vulkan" \
      +set r_vkValidation "${VK_VALIDATION}" \
      +set r_vkMeshShaders "${VK_MESH_SHADERS}" \
      +set r_vkRayTracing "${VK_RAY_TRACING}" \
      +set r_vkDevice "${VK_DEVICE}" \
      +set com_logfile "" \
      ${EXTRA_ARGS} \
      2>&1 | grep -v "LEAK:" | grep -v "Found.*GPU memory leaks" | grep -v "=== GPU Memory Leak Detection ===" | head -c 52428800 > "${BT_LOG}" || GDB_EXIT_CODE=$?
else
    # Fallback if timeout is not available
    echo "Warning: timeout command not found, running without timeout"
    gdb --batch -x "${GDB_CMDS}" --args "${EXE}" \
      +set fs_game "${GAME}" \
      +set com_error "1" \
      +set com_developer "1" \
      +set r_developer "1" \
      +set r_fullscreen "0" \
      +set r_mode "6" \
      +set r_windowed "1" \
      +set r_width "800" \
      +set r_height "600" \
      +set r_xpos "100" \
      +set r_ypos "100" \
      +set r_showFPS "1" \
      +set cl_renderer "vulkan" \
      +set r_vkValidation "${VK_VALIDATION}" \
      +set r_vkMeshShaders "${VK_MESH_SHADERS}" \
      +set r_vkRayTracing "${VK_RAY_TRACING}" \
      +set r_vkDevice "${VK_DEVICE}" \
      +set com_logfile "" \
      ${EXTRA_ARGS} \
      2>&1 | grep -v "LEAK:" | grep -v "Found.*GPU memory leaks" | grep -v "=== GPU Memory Leak Detection ===" | head -c 52428800 > "${BT_LOG}" || GDB_EXIT_CODE=$?
fi

# Check if gdb exited with an error
if [ "${GDB_EXIT_CODE}" -ne 0 ]; then
    echo "Warning: gdb exited with code ${GDB_EXIT_CODE}"
    if [ "${GDB_EXIT_CODE}" -eq 124 ]; then
        echo "This indicates a timeout (process ran longer than ${GDB_TIMEOUT} seconds)"
    else
        echo "This may indicate a crash or error"
    fi
fi

# Also display the output to console (limit to last 1000 lines to avoid flooding)
if [ -f "${BT_LOG}" ]; then
    LOG_SIZE=$(stat -f%z "${BT_LOG}" 2>/dev/null || stat -c%s "${BT_LOG}" 2>/dev/null || echo 0)
    if [ "${LOG_SIZE}" -gt 10485760 ]; then
        echo "Warning: Log file is large (${LOG_SIZE} bytes), showing last 1000 lines:"
        tail -1000 "${BT_LOG}"
    else
        cat "${BT_LOG}"
    fi
else
    echo "Warning: Log file not created: ${BT_LOG}"
fi

echo "[5/5] Done."
echo "If it crashed, the full backtrace is in: ${BT_LOG}"
echo "If you need to post-process a core manually:"
echo "  gdb ${EXE} core.* -ex 'set solib-search-path ${SOLIB_PATH}' -ex 'thread apply all bt full' -ex quit"

