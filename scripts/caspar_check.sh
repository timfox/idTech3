#!/usr/bin/env bash
# Validate SymForce Caspar availability (import + optional GPU smoke).
# Exit 0 if static checks pass; GPU steps warn and skip when no CUDA.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
SYMFORCE_DIR="${SYMFORCE_DIR:-$PROJECT_ROOT/external/symforce}"
VENV_DIR="${VENV_DIR:-$SYMFORCE_DIR/.venv}"

PASS=0
FAIL=0
WARN=0

pass() { PASS=$((PASS + 1)); echo "  ✓ $1"; }
fail() { FAIL=$((FAIL + 1)); echo "  ✗ $1" >&2; }
warn() { WARN=$((WARN + 1)); echo "  ! $1"; }

echo "=== Caspar / SymForce checks ==="

if [ -f "$PROJECT_ROOT/docs/CASPAR.md" ]; then
	pass "docs/CASPAR.md present"
else
	fail "docs/CASPAR.md missing"
fi

if grep -q 'external/symforce' "$PROJECT_ROOT/.gitmodules" 2>/dev/null; then
	pass ".gitmodules registers external/symforce"
else
	fail ".gitmodules missing symforce submodule"
fi

if [ -f "$SYMFORCE_DIR/symforce/experimental/caspar/README.md" ]; then
	pass "Caspar tree present under submodule"
else
	fail "Caspar not found (run init_optional_submodules.sh --symforce)"
fi

PYTHON=python3
if [ -f "$VENV_DIR/bin/activate" ]; then
	# shellcheck source=/dev/null
	source "$VENV_DIR/bin/activate"
	PYTHON=python
	pass "SymForce venv found ($VENV_DIR)"
else
	warn "No venv at $VENV_DIR (run build_symforce_caspar.sh)"
fi

if "$PYTHON" -c "from symforce.experimental.caspar import CasparLibrary; from symforce.experimental.caspar import memory" 2>/dev/null; then
	pass "Python import symforce.experimental.caspar"
else
	fail "Cannot import symforce.experimental.caspar (build_symforce_caspar.sh)"
fi

if command -v nvidia-smi >/dev/null 2>&1; then
	pass "nvidia-smi available"
	if "$PYTHON" -c "import torch; assert torch.cuda.is_available()" 2>/dev/null; then
		pass "PyTorch CUDA available"
		KERNEL_EX="$SYMFORCE_DIR/symforce/experimental/caspar/examples/kernel_example/gen_and_run.py"
		if [ -f "$KERNEL_EX" ]; then
			echo ""
			echo "  (optional) Running kernel_example gen_and_run.py ..."
			if ( cd "$(dirname "$KERNEL_EX")" && "$PYTHON" "$(basename "$KERNEL_EX")" ); then
				pass "kernel_example gen_and_run.py"
			else
				fail "kernel_example gen_and_run.py failed"
			fi
		fi
	else
		warn "PyTorch CUDA not available (install torch with CUDA in venv)"
	fi
else
	warn "No NVIDIA GPU / nvidia-smi (GPU Caspar tests skipped)"
fi

echo ""
echo "=== Results ==="
echo "  Passed: $PASS  Failed: $FAIL  Warnings: $WARN"
if [ "$FAIL" -gt 0 ]; then
	echo "CASPAR CHECK FAILED"
	exit 1
fi
echo "CASPAR CHECK PASSED"
exit 0
