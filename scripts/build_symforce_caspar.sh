#!/usr/bin/env bash
# Build SymForce from the optional submodule with Caspar (experimental) available.
#
# Usage:
#   ./scripts/build_symforce_caspar.sh
#   VENV_DIR=/tmp/sf-venv ./scripts/build_symforce_caspar.sh
#
# Environment:
#   SYMFORCE_DIR   - default: external/symforce
#   VENV_DIR       - default: external/symforce/.venv
#   PYTHON         - default: python3
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
SYMFORCE_DIR="${SYMFORCE_DIR:-$PROJECT_ROOT/external/symforce}"
VENV_DIR="${VENV_DIR:-$SYMFORCE_DIR/.venv}"
PYTHON="${PYTHON:-python3}"

if [ ! -f "$SYMFORCE_DIR/setup.py" ]; then
	echo "Error: SymForce source not found at $SYMFORCE_DIR" >&2
	echo "Run: ./scripts/init_optional_submodules.sh --symforce" >&2
	exit 1
fi

if ! command -v "$PYTHON" >/dev/null 2>&1; then
	echo "Error: $PYTHON not found" >&2
	exit 1
fi

echo "=== Building SymForce + Caspar ==="
echo "Source: $SYMFORCE_DIR"
echo "Venv:   $VENV_DIR"
echo ""

USE_VENV=1
if [ ! -d "$VENV_DIR" ]; then
	if ! "$PYTHON" -m venv "$VENV_DIR" 2>/dev/null; then
		echo "Warning: python venv unavailable (install python3-venv). Using pip --user." >&2
		USE_VENV=0
	fi
fi

if [ "$USE_VENV" -eq 1 ]; then
	# shellcheck source=/dev/null
	source "$VENV_DIR/bin/activate"
	PY_RUN=python
else
	PY_RUN="$PYTHON"
fi

"$PY_RUN" -m pip install -U pip wheel

REQ_BUILD="$SYMFORCE_DIR/requirements_build.txt"
if [ -f "$REQ_BUILD" ]; then
	"$PY_RUN" -m pip install -r "$REQ_BUILD"
fi

# Editable install so symforce.experimental.caspar is on the path.
if [ "$USE_VENV" -eq 0 ]; then
	"$PY_RUN" -m pip install --user -e "$SYMFORCE_DIR"
else
	"$PY_RUN" -m pip install -e "$SYMFORCE_DIR"
fi

echo ""
echo "Verifying Caspar import..."
"$PY_RUN" - <<'PY'
from symforce.experimental.caspar import CasparLibrary  # noqa: F401
print("OK: symforce.experimental.caspar.CasparLibrary")
PY

echo ""
if [ "$USE_VENV" -eq 1 ]; then
	echo "=== SymForce + Caspar ready in $VENV_DIR ==="
	echo "Activate: source $VENV_DIR/bin/activate"
else
	echo "=== SymForce + Caspar installed (pip --user) ==="
fi
echo "Check:    ./scripts/caspar_check.sh"
