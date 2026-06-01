#!/usr/bin/env bash
# Run SymForce Caspar BAL bundle-adjustment example (GPU + BAL dataset).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
SYMFORCE_DIR="${SYMFORCE_DIR:-$PROJECT_ROOT/external/symforce}"
VENV_DIR="${VENV_DIR:-$SYMFORCE_DIR/.venv}"
EXAMPLE="$SYMFORCE_DIR/symforce/experimental/caspar/examples/bal/gen_and_run.py"

if [ ! -f "$EXAMPLE" ]; then
	echo "Error: $EXAMPLE not found. Run ./scripts/init_optional_submodules.sh --symforce" >&2
	exit 1
fi

if [ -f "$VENV_DIR/bin/activate" ]; then
	# shellcheck source=/dev/null
	source "$VENV_DIR/bin/activate"
fi

echo "BAL example downloads data on first run; needs substantial GPU memory on large sets."
echo "See symforce/experimental/caspar/examples/bal/bal_loader.py"
echo ""

exec python "$EXAMPLE"
