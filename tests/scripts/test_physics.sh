#!/usr/bin/env bash
# Physics middleware smoke checks (Bullet + event bus + materials).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"
cd "$ROOT"

echo "[test_physics] checking middleware sources..."
PHYS_EVENTS="$(idtech3_require_file modules/physics/phys_events.c src/physics/phys_events.c)"
PHYS_MATERIALS="$(idtech3_require_file modules/physics/phys_materials.c src/physics/phys_materials.c)"
PHYS_MOTOR="$(idtech3_require_file modules/physics/phys_motor.c src/physics/phys_motor.c)"
PHYS_MIDDLEWARE="$(idtech3_require_file modules/physics/phys_middleware.c src/physics/phys_middleware.c)"
PHYS_DEBUGDRAW="$(idtech3_require_file modules/physics/phys_debugdraw.c src/physics/phys_debugdraw.c)"
CL_PHYS_DEBUG="$(idtech3_require_file runtime/client/cl_phys_debug.c src/client/cl_phys_debug.c)"
PHYS_BULLET_H="$(idtech3_require_file modules/physics/phys_bullet.h src/physics/phys_bullet.h)"
PHYS_PROC_ANIM="$(idtech3_require_file modules/physics/phys_procedural_anim.c src/physics/phys_procedural_anim.c)"

echo "[test_physics] grep API symbols..."
rg -q 'PhysEvent_Post' "$PHYS_EVENTS"
rg -q 'PhysMat_Get' "$PHYS_MATERIALS"
rg -q 'PhysMotor_UpdateAll' "$PHYS_MOTOR"
rg -q 'PhysMiddleware_Frame' "$PHYS_MIDDLEWARE"
rg -q 'Phys_ConvexSweep' "$PHYS_BULLET_H"
rg -q 'ProcAnim_UpdateAll' "$PHYS_PROC_ANIM"
rg -q 'CL_PhysDebugDrawSubmit' "$CL_PHYS_DEBUG"

echo "[test_physics] demo identity (no third-party game names in examples/)..."
if rg -i 'unwaking|open.?arena|q3dm' examples/ --glob '!examples/radiant/**' 2>/dev/null; then
	echo "examples/ contains third-party game references"
	exit 1
fi
test -f examples/demo_game/DEMO_IDENTITY.md
test -f examples/demo_game/mod/demo_physics.cfg

echo "[test_physics] ok"
