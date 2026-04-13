#!/usr/bin/env bash
set -euo pipefail

# Build minimal regression BSPs + pack z_renderer_regression.pk3 under docs/renderer_validation/devdata/rtest_base/.
# (Named rtest_base, not "base", so repo .gitignore rules for /base/ do not exclude it.)
#
# Requires ioquake3 qagame.qvm (GPL). Either:
#   export QAGAME_QVM=/path/to/qagame.qvm
# or build ioq3 once (CMake) and set:
#   export IOQ3_BUILD=/path/to/ioq3/build
# If unset, tries /tmp/ioq3-check/build/Release/baseq3/vm/qagame.qvm (agent bootstrap path).
#
# Usage: ./scripts/build_renderer_devdata.sh

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DEV_BASE="$PROJECT_ROOT/docs/renderer_validation/devdata/rtest_base"
GEN="$PROJECT_ROOT/scripts/tools/gen_rtest_bsp.py"

resolve_qagame() {
  if [ -n "${QAGAME_QVM:-}" ] && [ -f "$QAGAME_QVM" ]; then
    echo "$QAGAME_QVM"
    return
  fi
  if [ -n "${IOQ3_BUILD:-}" ] && [ -f "$IOQ3_BUILD/Release/baseq3/vm/qagame.qvm" ]; then
    echo "$IOQ3_BUILD/Release/baseq3/vm/qagame.qvm"
    return
  fi
  if [ -f /tmp/ioq3-check/build/Release/baseq3/vm/qagame.qvm ]; then
    echo /tmp/ioq3-check/build/Release/baseq3/vm/qagame.qvm
    return
  fi
  echo ""
}

QVM="$(resolve_qagame)"
if [ -z "$QVM" ]; then
  echo "Error: qagame.qvm not found. Build ioquake3 with BUILD_GAME_QVMS=ON, then:" >&2
  echo "  export QAGAME_QVM=/path/to/qagame.qvm" >&2
  echo "  or export IOQ3_BUILD=/path/to/ioq3/build" >&2
  exit 2
fi

rm -rf "$DEV_BASE"
mkdir -p "$DEV_BASE/maps" "$DEV_BASE/vm"

for m in rtest_tangent rtest_pbr rtest_emissive rtest_volumetric rtest_postfx rtest_parity; do
  python3 "$GEN" "$DEV_BASE/maps/${m}.bsp" --map-message "$m"
done

cp "$PROJECT_ROOT/docs/renderer_validation/devdata/default.cfg" "$DEV_BASE/default.cfg"
cp "$QVM" "$DEV_BASE/vm/qagame.qvm"

( cd "$DEV_BASE" && zip -q z_renderer_regression.pk3 default.cfg maps/*.bsp vm/qagame.qvm )

echo "Built $DEV_BASE and z_renderer_regression.pk3 ($(wc -c < "$DEV_BASE/z_renderer_regression.pk3") bytes)"
