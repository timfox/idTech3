# Physics Systems

## Bullet Physics (`phys_bullet.h/c` + `phys_bullet_impl.cpp`)

Full rigid body dynamics via Bullet3 (zlib license). 35 API functions.

**Compile:** `USE_BULLET_PHYSICS=ON` (default). Install `libbullet-dev` for C++ backend.

### Rigid Bodies
- Shapes: box, sphere, capsule, cylinder, convex hull, triangle mesh, compound, heightfield
- Types: static, dynamic, kinematic
- Forces, impulses, torques with contact points
- Collision groups and masks

### Constraints
- Point-to-point, hinge (with limits), cone-twist, fixed, generic 6DOF
- Dynamic limit adjustment

### Queries
- Ray cast with closest hit + body identification
- Sphere and box overlap tests

**Cvars:** `phys_enabled`, `phys_timestep`, `phys_maxSubSteps`, `phys_gravity`, `phys_debugDraw`

## Procedural Animation (`phys_procedural_anim.h/c`)

11-state controller layered on Bullet ragdolls: ANIMATED → BALANCE → STUMBLE → FALLING → BRACING → RAGDOLL → GETUP → DEAD + IMPACT, REACHING, GRABBED.

**Features:** Center-of-mass balance, spring-damper corrective forces, brace reactions (arms extend toward fall), head tracking, 8-slot IK targeting, muscle stiffness by state, consciousness/pain system, getup behavior.

**Config:** 16 parameters (balance stiffness/damping, stumble/fall thresholds, muscle min/max, brace timing, etc.)

## IK Solver (`phys_ik.h/c`)

- **Two-bone IK:** Cosine rule with pole vector (arms, legs)
- **CCD IK:** Iterative Cyclic Coordinate Descent for chains
- **Foot placement:** Two-bone wrapper with ground offset
- **Aim IK:** Bone rotation toward target with max angle
- **Look-at:** Head tracking with yaw/pitch limits

Quaternion utilities: axis-angle, multiply, slerp, rotate point.

## DMM Deformation (`phys_dmm.h/c` + `phys_dmm_materials.h/c`)

Digital Molecular Matter -- finite element deformation with fracture.

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

XPBD position-based dynamics. 4096 particles, 16384 constraints, 32 instances.

**Constraints:** Stretch, shear, bend (dihedral), long-range attachment.
**Wind:** Normal-dot-wind aerodynamics, turbulence noise, timed gusts.
**Pinning:** Individual particles, entire edges, movable pins for character-driven cloth.
**Sleep:** Automatic deactivation below motion threshold.
