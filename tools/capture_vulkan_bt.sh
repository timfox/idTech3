#!/usr/bin/env bash

# Automated helper to rebuild the Vulkan renderer with symbols, run it under gdb,
# and capture a full backtrace on crash. Outputs are saved under logs/.

set -euo pipefail

ROOT="/home/tim/Desktop/idtech3"
BUILD_DIR="${BUILD_DIR:-${ROOT}/build}"
SOLIB_PATH="${ROOT}/release"
EXE="${SOLIB_PATH}/idtech3.x86_64.so"
LOG_DIR="${LOG_DIR:-${ROOT}/logs}"

mkdir -p "${LOG_DIR}"

echo "[1/5] Enabling core dumps (current shell only)…"
ulimit -c unlimited

echo "[2/5] Configuring CMake with symbols (RelWithDebInfo, STRIP=OFF)…"
cmake -S "${ROOT}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSTRIP=OFF

echo "[3/5] Building targets: idtech3_vulkan_x86_64 and idtech3.x86_64…"
cmake --build "${BUILD_DIR}" --target idtech3_vulkan_x86_64
cmake --build "${BUILD_DIR}" --target idtech3.x86_64

timestamp="$(date +%Y%m%d_%H%M%S)"
BT_LOG="${LOG_DIR}/gdb_bt_${timestamp}.log"
GDB_CMDS="${LOG_DIR}/gdb_cmds_${timestamp}.gdb"

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

echo "[4/5] Running under gdb (logs: ${BT_LOG})…"
gdb --batch -x "${GDB_CMDS}" --args "${EXE}" +set fs_game mymod | tee "${BT_LOG}"

echo "[5/5] Done."
echo "If it crashed, the full backtrace is in: ${BT_LOG}"
echo "If you need to post-process a core manually:"
echo "  gdb ${EXE} core.* -ex 'set solib-search-path ${SOLIB_PATH}' -ex 'thread apply all bt full' -ex quit"

