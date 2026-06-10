#!/usr/bin/env bash
# RadiusFPS smoke test — CPU exactness vs reference FPS + optional Python benchmark.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${1:-${ROOT}/build-vk-Release}"

if [[ ! -x "${BUILD}/unit_radiusfps" ]]; then
  echo "SKIP: unit_radiusfps not built (cmake unit tests disabled or build missing)"
  exit 0
fi

"${BUILD}/unit_radiusfps"

if command -v python3 >/dev/null 2>&1; then
  python3 "${ROOT}/tools/radiusfps/benchmark.py" --quick
fi

echo "test_radiusfps.sh: OK"
