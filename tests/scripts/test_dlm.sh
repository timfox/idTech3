#!/usr/bin/env bash
# Deep-layered machine smoke test — C units + Python parity.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${1:-${ROOT}/build-vk-Release}"

if [[ ! -x "${BUILD}/unit_dlm" ]]; then
  echo "SKIP: unit_dlm not built (USE_RESEARCH_EXTENSIONS / full profile)"
  exit 0
fi

"${BUILD}/unit_dlm"

if command -v python3 >/dev/null 2>&1; then
  (cd "${ROOT}/tools/dlm" && python3 test_parity.py)
else
  echo "SKIP: python3 not found"
fi

echo "test_dlm.sh: OK"
