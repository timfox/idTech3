#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR="${ROOT}/build/examples/p2p_demo"

cmake -S "${ROOT}/examples/p2p_demo" -B "${BUILD_DIR}"
cmake --build "${BUILD_DIR}" -j "${CMAKE_BUILD_PARALLEL_LEVEL:-2}"

echo "Built ${BUILD_DIR}/p2p_demo"

