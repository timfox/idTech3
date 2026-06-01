#!/usr/bin/env bash
# Populate release/ for CI smoke_test + ctest after a raw CMake build (binaries only in bin/).
# Mirrors the non-binary parts of compile_engine.sh release staging.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
RELEASE_DIR="${1:-$PROJECT_ROOT/release}"
BIN_DIR="${2:-$PROJECT_ROOT/bin}"

mkdir -p "$RELEASE_DIR" "$RELEASE_DIR/examples"

shopt -s nullglob
for f in "$BIN_DIR"/idtech3*; do
	if [ -f "$f" ]; then
		cp -f "$f" "$RELEASE_DIR/"
	fi
done

for script in spec_energy_flux_generate.py trellis_image_to_glb.py run_vulkan.sh; do
	if [ -f "$PROJECT_ROOT/scripts/$script" ]; then
		cp -f "$PROJECT_ROOT/scripts/$script" "$RELEASE_DIR/"
		chmod +x "$RELEASE_DIR/$script" 2>/dev/null || true
	fi
done

for cfg in "$PROJECT_ROOT"/examples/q3_*.cfg "$PROJECT_ROOT"/examples/q3_fbo_safe.cfg; do
	if [ -f "$cfg" ]; then
		cp -f "$cfg" "$RELEASE_DIR/examples/"
	fi
done
