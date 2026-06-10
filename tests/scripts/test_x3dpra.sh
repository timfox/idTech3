#!/usr/bin/env bash
# x3DPRA smoke test — C units + Python parity + quick 2D reconstruct.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${1:-${ROOT}/build-vk-Release}"

if [[ -x "${ROOT}/scripts/install_research_python.sh" ]]; then
  bash "${ROOT}/scripts/install_research_python.sh" --ci-only || true
fi

if [[ ! -x "${BUILD}/unit_x3dpra" ]]; then
  echo "SKIP: unit_x3dpra not built"
  exit 0
fi

"${BUILD}/unit_x3dpra"

if command -v python3 >/dev/null 2>&1; then
  (cd "${ROOT}/tools/x3dpra" && python3 test_parity.py)
  if python3 -c "import numpy, scipy" 2>/dev/null; then
    (cd "${ROOT}/tools/x3dpra" && python3 validate_forward.py --write-fixture)
    (cd "${ROOT}/tools/x3dpra" && python3 validate_forward.py)
    (cd "${ROOT}/tools/x3dpra" && python3 benchmark_psnr.py --quick)
    (cd "${ROOT}/tools/x3dpra" && \
      out=$(python3 import_cst.py /nonexistent 2>&1 || true) && \
      echo "$out" | grep -q "CST import not configured")
    (cd "${ROOT}/tools/x3dpra" && python3 reconstruct.py \
      --mode 2d --object circle --solver lsqr_tv \
      --nx 30 --ny 30 --max-iter 40 --noise-db 0 --psnr-floor 17.0 \
      --output /tmp/x3dpra_smoke.npy)
  else
    echo "SKIP: numpy/scipy not installed for x3DPRA reconstruct"
  fi
else
  echo "SKIP: python3 not found"
fi

echo "test_x3dpra.sh: OK"
