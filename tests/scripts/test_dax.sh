#!/usr/bin/env bash
# DaX smoke test — C units + parity + mini-bench eval dry-run/full.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${1:-${ROOT}/build-vk-Release}"

if [[ -x "${ROOT}/scripts/install_research_python.sh" ]]; then
  bash "${ROOT}/scripts/install_research_python.sh" --ci-only || true
fi

if [[ ! -x "${BUILD}/unit_dax" ]]; then
  echo "SKIP: unit_dax not built"
  exit 0
fi

"${BUILD}/unit_dax"

if command -v python3 >/dev/null 2>&1; then
  (cd "${ROOT}/tools/dax" && python3 test_parity.py)
  (cd "${ROOT}/tools/dax" && python3 evaluate_benchmark.py --dry-run)
  if python3 -c "import numpy, sklearn" 2>/dev/null; then
    if python3 -c "import torch" 2>/dev/null; then
      (cd "${ROOT}/tools/dax" && python3 evaluate_benchmark.py \
        --manifest fixtures/mini_bench/tasks.json \
        --bench-root fixtures/mini_bench \
        --aggregation abmil)
    else
      (cd "${ROOT}/tools/dax" && python3 evaluate_benchmark.py \
        --manifest fixtures/mini_bench/tasks.json \
        --bench-root fixtures/mini_bench \
        --aggregation mean)
    fi
  else
    echo "SKIP: sklearn not installed for DaX mini-bench eval"
  fi
else
  echo "SKIP: python3 not found"
fi

echo "test_dax.sh: OK"
