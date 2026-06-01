#!/usr/bin/env bash
# Run SymForce Caspar kernel_example (requires GPU + built venv).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
SYMFORCE_DIR="${SYMFORCE_DIR:-$PROJECT_ROOT/external/symforce}"
VENV_DIR="${VENV_DIR:-$SYMFORCE_DIR/.venv}"
EXAMPLE="$SYMFORCE_DIR/symforce/experimental/caspar/examples/kernel_example/gen_and_run.py"

if [ ! -f "$EXAMPLE" ]; then
	echo "Error: $EXAMPLE not found. Run ./scripts/init_optional_submodules.sh --symforce" >&2
	exit 1
fi

if [ -f "$VENV_DIR/bin/activate" ]; then
	# shellcheck source=/dev/null
	source "$VENV_DIR/bin/activate"
else
	echo "Warning: venv missing; using system python (run build_symforce_caspar.sh)" >&2
fi

exec python "$EXAMPLE"
