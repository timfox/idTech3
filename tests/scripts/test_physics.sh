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
PHYS_PROPS="$(idtech3_require_file modules/physics/phys_props.c src/physics/phys_props.c)"
PHYS_VOLUMES="$(idtech3_require_file modules/physics/phys_volumes.c src/physics/phys_volumes.c)"
PHYS_DEBUGDRAW="$(idtech3_require_file modules/physics/phys_debugdraw.c src/physics/phys_debugdraw.c)"
CL_PHYS_DEBUG="$(idtech3_require_file runtime/client/shell/cl_phys_debug.c runtime/client/shell/cl_phys_debug.c)"
PHYS_BULLET_H="$(idtech3_require_file modules/physics/phys_bullet.h src/physics/phys_bullet.h)"
PHYS_PROPS_H="$(idtech3_require_file modules/physics/phys_props.h src/physics/phys_props.h)"
PHYS_PROC_ANIM="$(idtech3_require_file modules/physics/phys_procedural_anim.c src/physics/phys_procedural_anim.c)"

echo "[test_physics] grep API symbols..."
rg -q 'PhysEvent_Post' "$PHYS_EVENTS"
rg -q 'PhysMat_Get' "$PHYS_MATERIALS"
rg -q 'PhysMotor_UpdateAll' "$PHYS_MOTOR"
rg -q 'PhysMotor_GetActiveCount' "$PHYS_MOTOR"
rg -q 'Phys_RagdollApplyBoneTorque' "$PHYS_MOTOR"
rg -q 'PhysMiddleware_Frame' "$PHYS_MIDDLEWARE"
rg -q 'phys_spawn_ragdoll' "$PHYS_MIDDLEWARE"
rg -q 'phys_spawn_box' "$PHYS_MIDDLEWARE"
rg -q 'phys_impulse_sphere' "$PHYS_MIDDLEWARE"
rg -q 'phys_spawn_shadow' "$PHYS_MIDDLEWARE"
rg -q 'PhysProp_CreateShadow' "$PHYS_PROPS"
rg -q 'PhysProp_CreateFromAABB' "$PHYS_PROPS"
rg -q 'PHYS_VOLUME_BUOYANCY' "$PHYS_VOLUMES"
rg -q 'Phys_ApplyImpulseRadius' "$PHYS_BULLET_H"
rg -q 'Phys_IsBodyDynamic' "$PHYS_BULLET_H"
rg -q 'PHYS_GROUP_SHADOW' "$PHYS_PROPS_H"
rg -q 'Phys_ConvexSweep' "$PHYS_BULLET_H"
rg -q 'Phys_RagdollApplyBoneTorque' "$PHYS_BULLET_H"
rg -q 'Phys_GetRagdollCount' "$PHYS_BULLET_H"
rg -q 'ProcAnim_UpdateAll' "$PHYS_PROC_ANIM"
rg -q 'CL_PhysDebugDrawSubmit' "$CL_PHYS_DEBUG"

echo "[test_physics] backend switch + Box3D submodule..."
test -f modules/physics/phys_box3d_impl.c
rg -q 'IDTECH3_PHYSICS_BACKEND' CMakeLists.txt
rg -q 'USE_BOX3D_PHYSICS_IMPL' modules/physics/phys_box3d_impl.c
rg -q 'Phys_GetBackendName' modules/physics/phys_bullet.c
rg -q 'Phys_GetBackendName' "$PHYS_MIDDLEWARE"
if [ -f third_party/box3d/CMakeLists.txt ]; then
	rg -q 'box3d' third_party/box3d/CMakeLists.txt
else
	echo "[test_physics] note: third_party/box3d not checked out (submodule)"
fi

echo "[test_physics] demo cfg documents console path..."
rg -q 'phys_spawn_box' examples/demo_game/mod/demo_physics.cfg
rg -q 'phys_impulse_sphere' examples/demo_game/mod/demo_physics.cfg
rg -q 'phys_spawn_shadow' examples/demo_game/mod/demo_physics.cfg
rg -q 'phys_spawn_buoyancy' examples/demo_game/mod/demo_physics.cfg

rg -q 'b3World_CastShape' modules/physics/phys_box3d_impl.c
rg -q 'b3CreateMeshShape' modules/physics/phys_box3d_impl.c
rg -q 'b3World_Draw' modules/physics/phys_box3d_impl.c
rg -q 'b3World_Explode' modules/physics/phys_box3d_impl.c
rg -q 'b3World_CastMover' modules/physics/phys_box3d_impl.c
rg -q 'b3CreateCompound' modules/physics/phys_box3d_impl.c
rg -q 'b3CreateDistanceJoint' modules/physics/phys_box3d_impl.c
rg -q 'phys_workers' modules/physics/phys_bullet.c
rg -q 'Phys_AddStaticTriMesh_Impl' modules/physics/phys_impl.h
rg -q 'Phys_MoverStep' modules/physics/phys_character.c
rg -q 'phys_spawn_compound' modules/physics/phys_middleware.c
rg -q 'phys_spawn_cloth' modules/physics/phys_middleware.c
rg -q 'phys_spawn_rope' modules/physics/phys_middleware.c
rg -q 'phys_spawn_particles' modules/physics/phys_middleware.c
rg -q 'phys_spawn_softblob' modules/physics/phys_middleware.c
rg -q 'phys_spawn_fluid' modules/physics/phys_middleware.c
rg -q 'PhysFluid_CreateBlob' modules/physics/phys_fluid.c
rg -q 'PhysSolvers_Register' modules/physics/phys_solvers.c
rg -q 'PhysSolvers_PreStep' modules/physics/phys_bullet.c
rg -q 'PhysSolvers_PostStep' modules/physics/phys_bullet.c
rg -q 'SoftBlob_CreateLattice' modules/physics/phys_softblob.c
rg -q 'PhysParticles_CreateBurst' modules/physics/phys_particles.c
rg -q 'Cloth_CollideWorld' modules/physics/phys_cloth.c
rg -q 'PHYS_CONSTRAINT_DISTANCE' modules/physics/phys_bullet.h
rg -q '\"motors\"' modules/physics/phys_solvers.c
rg -q '\"fluid\"' modules/physics/phys_solvers.c

echo "[test_physics] docs omit third-party physics product names..."
if rg -i 'vphysics|havok|ipion|rubikon' docs/PHYSICS.md; then
	echo "docs/PHYSICS.md must not name third-party physics products"
	exit 1
fi
rg -q 'Box3D' docs/PHYSICS.md
rg -q 'IDTECH3_PHYSICS_BACKEND' docs/PHYSICS.md

echo "[test_physics] demo identity (no third-party game names in examples/)..."
if rg -i 'unwaking|open.?arena|q3dm' examples/ --glob '!examples/radiant/**' 2>/dev/null; then
	echo "examples/ contains third-party game references"
	exit 1
fi
test -f examples/demo_game/DEMO_IDENTITY.md
test -f examples/demo_game/mod/demo_physics.cfg

echo "[test_physics] ok"
