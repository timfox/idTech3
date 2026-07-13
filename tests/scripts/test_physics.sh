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
rg -q 'box3d_examples/menu.cfg' examples/demo_game/mod/demo_physics.cfg

echo "[test_physics] Box3D example scenes..."
BOX3D_EXAMPLES_DIR="examples/demo_game/mod/box3d_examples"
test -f "$BOX3D_EXAMPLES_DIR/menu.cfg"
test -f "$BOX3D_EXAMPLES_DIR/stacking_jenga.cfg"
test -f "$BOX3D_EXAMPLES_DIR/joints_bridge.cfg"
test -f "$BOX3D_EXAMPLES_DIR/events_sensors.cfg"
test -f "$BOX3D_EXAMPLES_DIR/continuous_bullets.cfg"
test -f "$BOX3D_EXAMPLES_DIR/compound_village.cfg"
test -f "$BOX3D_EXAMPLES_DIR/character_mover.cfg"
test -f "$BOX3D_EXAMPLES_DIR/softbody_fluid.cfg"
test -f "$BOX3D_EXAMPLES_DIR/replay_determinism.cfg"
rg -q 'stacking_jenga.cfg' "$BOX3D_EXAMPLES_DIR/menu.cfg"
rg -q 'phys_spawn_box' "$BOX3D_EXAMPLES_DIR/stacking_jenga.cfg"
rg -q 'phys_spawn_sphere' "$BOX3D_EXAMPLES_DIR/stacking_jenga.cfg"
rg -q 'phys_spawn_rope' "$BOX3D_EXAMPLES_DIR/joints_bridge.cfg"
rg -q 'phys_spawn_slider' "$BOX3D_EXAMPLES_DIR/joints_bridge.cfg"
rg -q 'phys_spawn_sensor' "$BOX3D_EXAMPLES_DIR/events_sensors.cfg"
rg -q 'phys_hitThreshold' "$BOX3D_EXAMPLES_DIR/events_sensors.cfg"
rg -q 'phys_ccd 1' "$BOX3D_EXAMPLES_DIR/continuous_bullets.cfg"
rg -q 'phys_spawn_heightfield' "$BOX3D_EXAMPLES_DIR/continuous_bullets.cfg"
rg -q 'phys_spawn_compound' "$BOX3D_EXAMPLES_DIR/compound_village.cfg"
rg -q 'phys_pmove 1' "$BOX3D_EXAMPLES_DIR/character_mover.cfg"
rg -q 'phys_spawn_shadow' "$BOX3D_EXAMPLES_DIR/character_mover.cfg"
rg -q 'phys_spawn_cloth' "$BOX3D_EXAMPLES_DIR/softbody_fluid.cfg"
rg -q 'phys_spawn_fluid' "$BOX3D_EXAMPLES_DIR/softbody_fluid.cfg"
rg -q 'phys_record_start' "$BOX3D_EXAMPLES_DIR/replay_determinism.cfg"
rg -q 'Box3D Example Scenes' docs/samples/box3d_examples/README.md

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
rg -q 'phys_spawn_fluid' modules/physics/phys_middleware.c
rg -q 'phys_spawn_sensor' modules/physics/phys_middleware.c
rg -q 'phys_spawn_slider' modules/physics/phys_middleware.c
rg -q 'phys_spawn_heightfield' modules/physics/phys_middleware.c
rg -q 'PhysFluid_CreateBlob' modules/physics/phys_fluid.c
rg -q 'b3CreatePrismaticJoint' modules/physics/phys_box3d_impl.c
rg -q 'b3CreateWheelJoint' modules/physics/phys_box3d_impl.c
rg -q 'b3CreateMotorJoint' modules/physics/phys_box3d_impl.c
rg -q 'b3CreateHeightField' modules/physics/phys_box3d_impl.c
rg -q 'b3Body_SetTargetTransform' modules/physics/phys_box3d_impl.c
rg -q 'b3World_GetSensorEvents' modules/physics/phys_box3d_impl.c
rg -q 'isSensor' modules/physics/phys_box3d_impl.c
rg -q 'Phys_AddStaticHeightField' modules/physics/phys_bullet.h
rg -q 'PHYS_CONSTRAINT_WHEEL' modules/physics/phys_bullet.h
rg -q 'PhysSolvers_Register' modules/physics/phys_solvers.c
rg -q 'PhysSolvers_PreStep' modules/physics/phys_bullet.c
rg -q 'PhysSolvers_PostStep' modules/physics/phys_bullet.c
rg -q 'SoftBlob_CreateLattice' modules/physics/phys_softblob.c
rg -q 'PhysParticles_CreateBurst' modules/physics/phys_particles.c
rg -q 'Cloth_CollideWorld' modules/physics/phys_cloth.c
rg -q 'PHYS_CONSTRAINT_DISTANCE' modules/physics/phys_bullet.h
rg -q '\"motors\"' modules/physics/phys_solvers.c
rg -q '\"fluid\"' modules/physics/phys_solvers.c

echo "[test_physics] full Box3D integration surface..."
rg -q 'SV_Physics_Frame' runtime/server/sv_physics.c
rg -q 'sv_physSpawn' runtime/server/sv_physics.c
rg -q 'EnginePhysMap_Parse' engine/core/engine_phys_map.c
rg -q 'misc_phys_box' engine/core/engine_phys_map.c
rg -q 'G_PHYS_CREATEBODY' runtime/game/g_public.h
rg -q 'G_PHYS_PMOVE_CORRECT' runtime/game/g_public.h
rg -q 'Phys_PmoveCorrect' modules/physics/phys_character.c
rg -q 'phys_pmove' modules/physics/phys_character.c
rg -q 'pmoveCorrect' runtime/game/g_lua_bindings.c
rg -q 'createConstraint' runtime/game/g_lua_bindings.c
rg -q 'b3CreateCylinder' modules/physics/phys_box3d_impl.c
rg -q 'b3CreateHull' modules/physics/phys_box3d_impl.c
rg -q 'b3Shape_SetFilter' modules/physics/phys_box3d_impl.c
rg -q 'b3World_CastRayClosest' modules/physics/phys_box3d_impl.c
rg -q 'b3World_OverlapShape' modules/physics/phys_box3d_impl.c
rg -q 'b3Joint_SetForceThreshold' modules/physics/phys_box3d_impl.c
rg -q 'b3WheelJoint_SetTargetSteeringAngle' modules/physics/phys_box3d_impl.c
rg -q 'b3World_GetProfile' modules/physics/phys_box3d_impl.c
rg -q 'b3World_StartRecording' modules/physics/phys_box3d_impl.c
rg -q 'Phys_RagdollSetBoneAnimTarget' modules/physics/phys_bullet.h
rg -q 'Phys_RagdollLoadDef' modules/physics/phys_ragdoll_bind.c
rg -q 'Phys_RagdollSpawnBound' modules/physics/phys_ragdoll_bind.c
rg -q 'Phys_RagdollApplyMd3Frame' modules/physics/phys_ragdoll_bind.c
rg -q 'phys_stepHeight' modules/physics/phys_box3d_impl.c
rg -q 'G_PHYS_CREATERAGDOLL' runtime/game/g_public.h
rg -q 'CG_PHYS_CREATERAGDOLL' runtime/cgame/cg_public.h
rg -q 'createRagdoll' runtime/game/g_lua_bindings.c
rg -q 'subscribe' runtime/game/g_lua_bindings.c
rg -q 'phys_spawn_ragdoll_bind' modules/physics/phys_middleware.c
rg -q 'PHYS_EVENT_CONTACT_BEGIN' modules/physics/phys_events.h
rg -q 'PHYS_EVENT_BODY_SLEEP' modules/physics/phys_events.h
rg -q 'b3World_GetBodyEvents' modules/physics/phys_box3d_impl.c
rg -q 'events.beginCount' modules/physics/phys_box3d_impl.c
rg -q 'b3ValidateReplay' modules/physics/phys_box3d_impl.c
rg -q 'b3Shape_SetFriction' modules/physics/phys_box3d_impl.c
rg -q 'phys_set_friction' modules/physics/phys_middleware.c
rg -q 'setFriction' runtime/game/g_lua_bindings.c
rg -q 'validateReplay' runtime/game/g_lua_bindings.c
rg -q 'pollEvent' runtime/game/g_lua_bindings.c
rg -q 'PhysEvent_Poll' modules/physics/phys_events.c
rg -q 'Phys_RayCastFiltered' modules/physics/phys_bullet.h
rg -q 'Phys_GetBodyContacts' modules/physics/phys_bullet.h
rg -q 'PHYS_CONSTRAINT_FILTER' modules/physics/phys_bullet.h
rg -q 'PHYS_CONSTRAINT_PARALLEL' modules/physics/phys_bullet.h
rg -q 'b3CreateFilterJoint' modules/physics/phys_box3d_impl.c
rg -q 'b3CreateParallelJoint' modules/physics/phys_box3d_impl.c
rg -q 'b3Body_GetContactData' modules/physics/phys_box3d_impl.c
rg -q 'b3World_SetHitEventThreshold' modules/physics/phys_box3d_impl.c
rg -q 'Phys_SetConstraintSpring' modules/physics/phys_bullet.h
rg -q 'phys_set_filter' modules/physics/phys_middleware.c
rg -q 'phys_dump' modules/physics/phys_middleware.c
rg -q 'phys_hitThreshold' modules/physics/phys_bullet.c
rg -q 'ProcAnim_UpdateGround|IK_SolveFootPlacement|Euphoria' modules/physics/phys_procedural_anim.c
rg -q 'PhysMotor_RunGetup' modules/physics/phys_motor.c
rg -q 'Dmm_SpawnFragments' modules/physics/phys_bullet.h
rg -q 'phys_spawn_dmm' modules/physics/phys_middleware.c
rg -q 'd\.name, "dmm"' modules/physics/phys_solvers.c
rg -q 'Dmm_UpdateAll' modules/physics/phys_dmm.c
rg -q 'Phys_RagdollSpawnBoundEx' modules/physics/phys_ragdoll_bind.c
rg -q 'ENGINE_PHYS_DMM' engine/core/engine_phys_map.h
rg -q 'func_destructible' engine/core/engine_phys_map.c
rg -q 'PhysMotor_FindByRagdoll' modules/physics/phys_motor.c
rg -q 'ProcAnim_FindByRagdoll' modules/physics/phys_procedural_anim.c
rg -q 'BOX_UD_RAG_FLAG' modules/physics/phys_box3d_impl.c
rg -q 'spawnBoundAlive' runtime/game/g_lua_bindings.c
rg -q 'createDmm' runtime/game/g_lua_bindings.c
rg -q 'supportCenter' modules/physics/phys_procedural_anim.h
rg -q 'Phys_GetClosestPoint' modules/physics/phys_bullet.h
rg -q 'Phys_SphereTimeOfImpact' modules/physics/phys_box3d_impl.c
rg -q 'Phys_SetCustomFilterCallback' modules/physics/phys_bullet.h
rg -q 'b3World_SetCustomFilterCallback' modules/physics/phys_box3d_impl.c
rg -q 'b3RecPlayer_SeekFrame' modules/physics/phys_box3d_impl.c
rg -q 'Phys_UpdateStaticTriMesh' modules/physics/phys_box3d_impl.c
rg -q 'b3World_RebuildStaticTree' modules/physics/phys_box3d_impl.c
rg -q 'b3Body_SetBullet' modules/physics/phys_box3d_impl.c
rg -q 'Phys_SetHingeTargetAngle' modules/physics/phys_bullet.h
rg -q 'phys_replay_open' modules/physics/phys_middleware.c
rg -q 'getClosestPoint' runtime/game/g_lua_bindings.c
rg -q 'sphereTOI' runtime/game/g_lua_bindings.c
rg -q 'meshes\[256\]' modules/physics/phys_box3d_impl.c
rg -q 'phys_bspGridStep' modules/physics/phys_bullet.c
rg -q 'misc_phys_box' docs/PHYSICS.md
rg -q 'Euphoria' docs/PHYSICS.md
rg -q 'DMM-like' docs/PHYSICS.md
rg -q 'RecPlayer' docs/PHYSICS.md
rg -q 'Closest-point' docs/PHYSICS.md
rg -q 'misc_phys_dmm' docs/EDITOR_BRIDGE.md
rg -q 'func_destructible' docs/EDITOR_BRIDGE.md
rg -q 'misc_phys_box' examples/radiant/scripts/entities_idtech3_bridge.def
rg -q 'misc_phys_dmm' examples/radiant/scripts/entities_idtech3_bridge.def

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
