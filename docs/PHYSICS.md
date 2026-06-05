# Physics Systems

## Architecture

Bullet is the **collision / constraint / impulse substrate**. Gameplay-facing behavior lives in middleware above it:

```txt
id Tech 3 game / cgame / Lua
    ↓
PhysMiddleware (hits, materials, motors, ProcAnim tick)
    ↓
Bullet (rigid bodies, ragdolls, soft-body DMM, queries)
    ↓
Vulkan debug lines (phys_debugDraw → CL_PhysDebugDrawSubmit)
```

| Layer | Module | Role |
|-------|--------|------|
| Substrate | `phys_bullet*` | Rigid bodies, constraints, ragdolls, ray/sweep queries |
| Euphoria-like | `phys_procedural_anim`, `phys_motor` | Balance, stumble, brace, PD motors, per-bone physics blend |
| DMM-like | `phys_dmm*`, `phys_materials` | Stress/fracture + gameplay material table |
| XPBD secondary | `phys_cloth` | Ropes/cloth/vines beside Bullet (not inside it) |
| Event bus | `phys_events` | Impact, break, splash, balance-lost → audio/particles/AI |
| Orchestration | `phys_middleware` | Per-frame tick, hit routing, `phys_status` |

## Bullet Physics (`phys_bullet.h/c` + `phys_bullet_impl.cpp`)

Full rigid body dynamics via Bullet3 (zlib license). Uses `btSoftRigidDynamicsWorld` (rigid + soft-body DMM).

**Compile:** `USE_BULLET_PHYSICS=ON` (default). Install `libbullet-dev` for C++ backend.

### Rigid Bodies
- Shapes: box, sphere, capsule, cylinder, convex hull, triangle mesh, compound, heightfield
- Types: static, dynamic, kinematic
- Forces, impulses, torques with contact points
- Collision groups and masks
- Per-body `materialId` (`PHYS_MAT_*` from `phys_materials.h`)

### Constraints
- Point-to-point, hinge (with limits), cone-twist, fixed, generic 6DOF
- Dynamic limit adjustment

### Queries
- Ray cast with closest hit + body identification
- **Convex sweep** (`Phys_ConvexSweep`) for capsule/box probes
- Sphere and box overlap tests

### Debug
- `phys_debugDraw 1` — Bullet wireframe → `PhysDebug` line buffer → Vulkan polys

**Cvars:** `phys_enabled`, `phys_timestep`, `phys_maxSubSteps`, `phys_gravity`, `phys_debugDraw`, `phys_events`, `phys_motor`

**Commands:** `phys_status`

## Gameplay Materials (`phys_materials.h/c`)

Unified `phys_material_t` table (wood, glass, metal, concrete, flesh, mud, water, …) driving:

- Bullet friction/restitution/density (`PhysMat_ApplyToBodyDef`)
- Impact response (particles, sound, decals, fracture stress)
- DMM type mapping (`PhysMat_FromDmmType`)

## Physics Event Bus (`phys_events.h/c`)

```c
PhysEvent_Subscribe(PHYS_EVENT_IMPACT, handler, userData);
PhysEvent_PostImpact(...);
PhysEvent_PostCharacterHit(...);
PhysEvent_DispatchQueued();  /* each frame from PhysMiddleware_Frame */
```

Event types: `IMPACT`, `BREAK`, `DENT`, `SPLASH`, `FALL`, `BALANCE_LOST`, `RAGDOLL_SLEEP`, `GRAB`, `TEAR`.

Contact manifolds above impulse threshold auto-post `PHYS_EVENT_IMPACT` after each step.

## Active Ragdoll Motors (`phys_motor.h/c`)

Euphoria-like PD motor layer on ProcAnim + Bullet ragdolls:

- Controllers: balance, protect-head, brace, stumble, pain
- `joint_motor_cmd_t` per bone (stiffness, damping, target orientation)
- Per-bone physics blend weights (head/arms/legs)
- `PhysMotor_ApplyHit` routes `phys_hit_event_t` into ProcAnim

Pair with ProcAnim:

```c
procAnimHandle_t anim = ProcAnim_Create(ragdoll, &config);
physMotorHandle_t motor = PhysMotor_Create(anim, ragdoll);
PhysMiddleware_DispatchHit(entityNum, anim, motor, bone, damageType, point, impulse);
```

## Procedural Animation (`phys_procedural_anim.h/c`)

11-state controller layered on Bullet ragdolls: ANIMATED → BALANCE → STUMBLE → FALLING → BRACING → RAGDOLL → GETUP → DEAD + IMPACT, REACHING, GRABBED.

**Features:** Center-of-mass balance, spring-damper corrective forces, brace reactions (arms extend toward fall), head tracking, 8-slot IK targeting, muscle stiffness by state, consciousness/pain system, getup behavior.

**Tick:** `ProcAnim_UpdateAll(dt)` from `PhysMiddleware_Frame` (client game frame).

**Config:** 16 parameters (balance stiffness/damping, stumble/fall thresholds, muscle min/max, brace timing, etc.)

## IK Solver (`phys_ik.h/c`)

- **Two-bone IK:** Cosine rule with pole vector (arms, legs)
- **CCD IK:** Iterative Cyclic Coordinate Descent for chains
- **Foot placement:** Two-bone wrapper with ground offset
- **Aim IK:** Bone rotation toward target with max angle
- **Look-at:** Head tracking with yaw/pitch limits

Quaternion utilities: axis-angle, multiply, slerp, rotate point.

## DMM Deformation (`phys_dmm.h/c` + `phys_dmm_materials.h/c`)

Digital Molecular Matter — finite element deformation with fracture (DMM-like, staged above Bullet).

### Materials (12 presets)
Wood, glass, thin/thick metal, concrete, stone, ice, plastic, cloth, rubber, flesh. Each with density, Young's modulus, Poisson's ratio, yield/ultimate strength, fracture energy, thermal properties.

### Fracture Modes
Voronoi, radial, splinter (wood), shatter (glass), slice, crumble (concrete), tear (metal), peel.

### Constraint Solver
1024 nodes, 2048 constraints. Verlet integration + iterative relaxation. Auto-grid generation. Per-constraint break thresholds.

### Thermal
Temperature tracking, softening/melting points, heat conduction, cool-down.

### 10 Prefabs
WoodenDoor, GlassPane, MetalBarrel, ConcreteWall, IceBlock, WoodenCrate, MetalGrate, BrickWall, Railing, TreeTrunk.

## Cloth Simulation (`phys_cloth.h/c`)

XPBD position-based dynamics beside Bullet (ropes, vines, cloth straps). 4096 particles, 16384 constraints, 32 instances.

**Constraints:** Stretch, shear, bend (dihedral), long-range attachment.
**Wind:** Normal-dot-wind aerodynamics, turbulence noise, timed gusts.
**Pinning:** Individual particles, entire edges, movable pins for character-driven cloth.
**Sleep:** Automatic deactivation below motion threshold.

## Roadmap (phased)

1. **Phase 1 (done):** Bullet world, capsule character, props, ray/sweep, debug draw, materials, event bus
2. **Phase 2 (partial):** Ragdoll + ProcAnim + motors + balance/recovery wired in game frame
3. **Phase 3:** Prefractured meshes, stress accumulation, persistent damage
4. **Phase 4:** Swamp buoyancy, mud drag, vine XPBD coupling
5. **Phase 5:** Vulkan compute debris/particles, interaction zones
