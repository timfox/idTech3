#!/usr/bin/env bash
# Domany–Kinzel QSD smoke test — C units + optional Python dense parity.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${1:-${ROOT}/build-vk-Release}"

if [[ ! -x "${BUILD}/unit_dk_qsd" ]]; then
  echo "SKIP: unit_dk_qsd not built (USE_RESEARCH_EXTENSIONS / full profile)"
  exit 0
fi

"${BUILD}/unit_dk_qsd"

if command -v python3 >/dev/null 2>&1; then
  (cd "${ROOT}/tools/dk_qsd" && python3 test_parity.py)
else
  echo "SKIP: python3 not found"
fi

echo "test_dk_qsd.sh: OK"
