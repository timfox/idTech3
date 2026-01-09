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

clear

mkdir -p "${LOG_DIR}"

echo "[1/5] Enabling core dumps (current shell only)…"
ulimit -c unlimited

echo "[2/5] Configuring CMake with symbols (RelWithDebInfo)…"
cmake -S "${ROOT}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=RelWithDebInfo

echo "[3/5] Building targets: idtech3_vulkan_x86_64 and idtech3.x86_64…"
cmake --build "${BUILD_DIR}" --target idtech3_vulkan_x86_64
cmake --build "${BUILD_DIR}" --target idtech3.x86_64

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
# Set breakpoint on fatal error function to catch before exit
break ri.Error
run
# If we hit the breakpoint or a signal, capture context
if $_isvoid($pc) == 0
  printf "\n== Crash context (process stopped) ==\n"
  printf "PC=%p\n", $pc
  info symbol $pc
  printf "\n== Full backtrace ==\n"
  thread apply all bt full
  printf "\n== Register state ==\n"
  info registers
  printf "\n== Local variables ==\n"
  info locals
  continue
else
  printf "\n== Process exited ==\n"
  printf "Check output above for error message\n"
end
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
# Run gdb and capture output to file directly to avoid stderr blocking issues
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
  ${EXTRA_ARGS} \
  > "${BT_LOG}" 2>&1

# Also display the output to console
cat "${BT_LOG}"

echo "[5/5] Done."
echo "If it crashed, the full backtrace is in: ${BT_LOG}"
echo "If you need to post-process a core manually:"
echo "  gdb ${EXE} core.* -ex 'set solib-search-path ${SOLIB_PATH}' -ex 'thread apply all bt full' -ex quit"

