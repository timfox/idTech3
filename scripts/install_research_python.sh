#!/usr/bin/env bash
# Install Python dependencies for research-module smoke tests.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MODE="${1:---ci-only}"

need() {
  python3 - "$@" <<'PY'
import importlib, sys
mods = sys.argv[1:]
for m in mods:
    try:
        importlib.import_module(m)
    except ImportError:
        sys.exit(1)
PY
}

pip_install() {
  if python3 -m pip install --user -q "$@"; then
    return 0
  fi
  python3 -m pip install -q --break-system-packages "$@" || return 1
}

install_file() {
  local req="$1"
  if need numpy scipy sklearn 2>/dev/null; then
    echo "[research-python] CI deps already present"
    return 0
  fi
  pip_install -r "$req" || {
    echo "[research-python] WARN pip install failed; ensure numpy scipy scikit-learn are available"
    return 0
  }
}

case "$MODE" in
  --ci-only|--minimal)
    echo "[research-python] installing CI-minimal deps..."
    install_file "${ROOT}/tools/research_requirements-ci.txt"
    ;;
  --full)
    echo "[research-python] installing CI + module requirements..."
    install_file "${ROOT}/tools/research_requirements-ci.txt"
    pip_install -r "${ROOT}/tools/x3dpra/requirements.txt" || true
    pip_install -r "${ROOT}/tools/dax/requirements.txt" || true
    pip_install -r "${ROOT}/tools/gccfer/requirements.txt" || true
    ;;
  --torch)
    echo "[research-python] installing CPU torch for GCC-FER/DaX smokes..."
    install_file "${ROOT}/tools/research_requirements-ci.txt"
    pip_install torch torchvision --index-url https://download.pytorch.org/whl/cpu || true
    ;;
  *)
    echo "Usage: $0 [--ci-only|--full|--torch]" >&2
    exit 1
    ;;
esac

echo "[research-python] OK"
