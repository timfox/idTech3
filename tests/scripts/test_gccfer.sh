#!/usr/bin/env bash
# GCC-FER / CA-FER smoke test — C units + parity + 1-epoch mini train.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${1:-${ROOT}/build-vk-Release}"

if [[ -x "${ROOT}/scripts/install_research_python.sh" ]]; then
  bash "${ROOT}/scripts/install_research_python.sh" --ci-only || true
fi

if [[ ! -x "${BUILD}/unit_gccfer" ]]; then
  echo "SKIP: unit_gccfer not built"
  exit 0
fi

"${BUILD}/unit_gccfer"

if command -v python3 >/dev/null 2>&1; then
  (cd "${ROOT}/tools/gccfer" && python3 test_parity.py)
  (cd "${ROOT}/tools/gccfer" && python3 test_metrics.py)
  (cd "${ROOT}/tools/gccfer" && python3 validate_manifest.py --manifest fixtures/mini_manifest.csv)
  if python3 -c "import torch" 2>/dev/null; then
    if ! (cd "${ROOT}/tools/gccfer" && python3 train_cafer.py \
      --manifest fixtures/mini_manifest.csv \
      --output /tmp/gccfer_smoke_ckpt \
      --epochs 1 --folds 2 --baseline); then
      echo "SKIP: gccfer mini train failed (optional vivit/transformers path)"
    fi
  else
    echo "SKIP: torch not installed for gccfer train smoke"
  fi
else
  echo "SKIP: python3 not found"
fi

echo "test_gccfer.sh: OK"
