#!/usr/bin/env bash

# Automated helper to rebuild the Vulkan renderer with symbols, run it under gdb,
# and capture a full backtrace on crash. Outputs are saved under logs/.

set -euo pipefail

ROOT="/home/tim/Desktop/idtech3"
BUILD_DIR="${BUILD_DIR:-${ROOT}/build}"
SOLIB_PATH="${ROOT}/release"
EXE="${SOLIB_PATH}/idtech3.x86_64.so"
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
run
printf "\n== Crash context ==\n"
printf "PC=%p\n", $pc
info symbol $pc
info sharedlibrary idtech3_vulkan_x86_64.so
info files
maintenance info sections ?idtech3_vulkan_x86_64.so
printf "\n== Full backtrace ==\n"
thread apply all bt full
quit
EOF
  sed -i "s|__SOLIB__|${SOLIB_PATH}|g" "${GDB_CMDS}"
else
  GDB_CMDS="${GDB_CMDS_OVERRIDE}"
fi

echo "[4/5] Running under gdb using ${GDB_CMDS} (logs: ${BT_LOG})…"
gdb --batch -x "${GDB_CMDS}" --args "${EXE}" \
  +set fs_game "${GAME}" \
  +set r_vkValidation "${VK_VALIDATION}" \
  +set r_vkMeshShaders "${VK_MESH_SHADERS}" \
  +set r_vkRayTracing "${VK_RAY_TRACING}" \
  +set r_vkDevice "${VK_DEVICE}" \
  ${EXTRA_ARGS} \
  | tee "${BT_LOG}"

echo "[5/5] Done."
echo "If it crashed, the full backtrace is in: ${BT_LOG}"
echo "If you need to post-process a core manually:"
echo "  gdb ${EXE} core.* -ex 'set solib-search-path ${SOLIB_PATH}' -ex 'thread apply all bt full' -ex quit"

