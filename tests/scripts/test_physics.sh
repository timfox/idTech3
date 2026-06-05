#!/usr/bin/env bash
# Physics middleware smoke checks (Bullet + event bus + materials).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

echo "[test_physics] checking middleware sources..."
for f in \
	src/physics/phys_events.c \
	src/physics/phys_materials.c \
	src/physics/phys_motor.c \
	src/physics/phys_middleware.c \
	src/physics/phys_debugdraw.c \
	src/client/cl_phys_debug.c
do
	test -f "$f" || { echo "missing $f"; exit 1; }
done

echo "[test_physics] grep API symbols..."
rg -q 'PhysEvent_Post' src/physics/phys_events.c
rg -q 'PhysMat_Get' src/physics/phys_materials.c
rg -q 'PhysMotor_UpdateAll' src/physics/phys_motor.c
rg -q 'PhysMiddleware_Frame' src/physics/phys_middleware.c
rg -q 'Phys_ConvexSweep' src/physics/phys_bullet.h
rg -q 'ProcAnim_UpdateAll' src/physics/phys_procedural_anim.c
rg -q 'CL_PhysDebugDrawSubmit' src/client/cl_phys_debug.c

echo "[test_physics] demo identity (no third-party game names in examples/)..."
if rg -i 'unwaking|open.?arena|quake.?3|q3dm' examples/ 2>/dev/null; then
	echo "examples/ contains third-party game references"
	exit 1
fi
test -f examples/demo_game/DEMO_IDENTITY.md
test -f examples/demo_game/mod/demo_physics.cfg

echo "[test_physics] ok"
