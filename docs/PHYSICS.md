# Physics Systems

## Architecture

**Box3D Soft Step** ([timfox/idTech3-box3d](https://github.com/timfox/idTech3-box3d)) is the default **rigid substrate** (`third_party/box3d`). Companion solvers share that world through `Phys_*`. Bullet and **Jolt Physics** ([jrouwe/JoltPhysics](https://github.com/jrouwe/JoltPhysics)) are optional alternates, with Jolt currently wired as an integration scaffold while the full gameplay bridge is completed.

```txt
Phys_StepSimulation
  ├─ PhysSolvers_PreStep (shadows / volumes / procanim / motors)
  ├─ Soft Step (Box3D)     ← primary rigid solver
  ├─ contact + sensor + joint-break events
  └─ PhysSolvers_PostStep (cloth / particles / softblob / fluid)
```

### Authority

| Host | Soft Step tick |
|------|----------------|
| Dedicated server | `SV_Physics_Frame` when `phys_enabled` |
| Listen / client | `CL_GameFrame` (listen skips server double-step) |
| Map props | `sv_physSpawn 1` → `misc_phys_*` via `EnginePhysMap_Parse` |

Classic Q3/OA Pmove stays default (`phys_pmove 0`). Set `phys_pmove 1` for CastMover correction (`Phys_PmoveCorrect` / `G_PHYS_PMOVE_CORRECT` / Lua `Engine.Physics.pmoveCorrect`).

## Box3D substrate coverage

| Feature | Status |
|---------|--------|
| Soft Step + workers / sleep / CCD | Done |
| Bodies (static / dynamic / kinematic) | Done |
| True cylinder + convex hull | Done |
| Multi-shape attach / destroy | Done |
| Runtime shape filters | Done |
| Sensors (`isSensor` → `MOTION_ENTER/EXIT`) | Done |
| Prismatic / wheel / motor / distance / revolute / weld / spherical | Done |
| Joint break thresholds → `PHYS_EVENT_BREAK` | Done |
| Wheel steering setters | Done |
| Height field (Z-up via body rotation) | Done |
| Mesh + compound static | Done |
| CastRayClosest / OverlapShape | Done |
| CastMover character + opt-in Pmove | Done |
| Kinematic `SetTargetTransform` (shadows) | Done |
| Gravity scale / motion locks | Done |
| Explode / debug draw / hit events | Done |
| MD3 / `.rag` ragdoll bind + anim blend | Done |
| Euphoria-like active ragdoll (ProcAnim + motors) | Done (ground plant, stumble IK, Z-up getup) |
| DMM-like Soft Step fracture companion | Done (proxy body, stress, debris) |
| Profile counters (`phys_status`) | Done |
| Contact begin/end (non-sensor) | Done (`PHYS_EVENT_CONTACT_*` + manifold point) |
| Body sleep events | Done (`PHYS_EVENT_BODY_SLEEP`) |
| Recording + replay validate | Done (`phys_record_*` / `phys_replay`) |
| Runtime friction / restitution | Done (`phys_set_friction` / `Phys_SetBodyFriction`) |
| Query filter masks | Done (`Phys_*Filtered` / Lua filter args) |
| Contact manifolds poll | Done (`Phys_GetBodyContacts` / Lua `getContacts`) |
| Filter + parallel joints | Done |
| Joint springs / spherical limits / reaction | Done |
| Script event poll | Done (`PhysEvent_Poll` / Lua `pollEvent`) |
| Hit threshold + world dump | Done (`phys_hitThreshold` / `phys_dump`) |
| Closest-point query | Done (`Phys_GetClosestPoint` / `phys_closest`) |
| Sphere TOI / shape cast | Done (`Phys_SphereTimeOfImpact` / Soft Step CastShape) |
| Custom filter + pre-solve callbacks | Done (`Phys_SetCustomFilterCallback` / `Phys_SetPreSolveCallback`) |
| Interactive RecPlayer seek/step | Done (`phys_replay_open|step|seek|close`) |
| Static mesh update + tree rebuild | Done (`Phys_UpdateStaticTriMesh` / `Phys_RebuildStaticTree`, 256 mesh slots) |
| Per-body CCD (bullet) + sleep | Done (`Phys_SetBodyContinuous` / `Phys_SetBodySleepEnabled`) |
| World contact / speed / speculative tuning | Done (`phys_contactHertz`, `phys_maxLinearSpeed`, `phys_speculative`) |
| Debug draw contacts / flags | Done (`phys_debug_flags` / `phys_debugContacts`) |
| Joint servo targets (hinge/slider/distance) | Done (`Phys_SetHingeTargetAngle` / `SetSliderTarget` / `SetDistanceLength`) |
| Wheel suspension / spin | Done (`Phys_SetWheelSuspension` / `Phys_SetWheelSpin`) |
| Motor joint 3D velocities | Done (`Phys_SetMotorVelocities`) |
| Spherical joint target orientation | Done (`Phys_SetSphericalTarget`) |
| Body damping / type / wind | Done (`Phys_SetBodyDamping` / `SetBodyType` / `ApplyWind`) |
| Explode with maskBits | Done (`Phys_Explode` / `phys_explode`) |
| Multi-hit ray cast | Done (`Phys_RayCastAll` / `phys_ray_all`) |
| Friction / restitution mix callbacks | Done (`Phys_SetFrictionCallback` / `Phys_SetRestitutionCallback`) |

### Optional / MED–LOW (not blockers)

| API | Notes |
|-----|-------|
| Full FEM soft bodies | Not in Box3D — keep XPBD/DMM companions ([timfox/idTech3-box3d](https://github.com/timfox/idTech3-box3d) Soft Step is rigid) |
| Open-world sector mesh stream | `UpdateStaticTriMesh` + `RebuildStaticTree` ready; sector residency hookup follow-on |

## Multi-solver companions

| Name | Phase | Console |
|------|-------|---------|
| `shadows` / `volumes` / `procanim` / `motors` | PRE | Euphoria-like: `phys_spawn_ragdoll`, `phys_hit_ragdoll` |
| `dmm` | POST | DMM-like: `phys_spawn_dmm`, `phys_hit_dmm` |
| `xpbd_cloth` / `particles` / `softblob` / `fluid` | POST | `phys_spawn_*` |

```bash
phys_solvers
phys_spawn_ragdoll 0 0 64
phys_hit_ragdoll 0 500
phys_spawn_dmm 0 0 48
phys_hit_dmm 1200
phys_status
```

### Euphoria-like (active ragdoll)

Soft Step capsules + spherical joints driven by **ProcAnim** (balance / stumble / brace / getup) and **PhysMotor** (per-bone PD torques). Foot raycasts set `onGround`; stumble uses foot-placement IK → `Phys_RagdollReach`. Toggle: `phys_motor`.

### DMM-like (destructible props)

Not FEM. Soft Step **rigid proxy** + stress grid; contacts / impacts accumulate strain; on fracture `Dmm_SpawnFragments` replaces the proxy with debris boxes. Cvars: `phys_dmm_enabled`, `phys_dmm_resolution`, `phys_dmm_fracture`. Soft materials still use `xpbd_cloth` / `softblob`.

Parker & O’Brien SCA 2009 (Pixelux trade-name DMM / Force Unleashed) describes **corotational tet FEM + fracture** — that design is documented as a research scaffold in **[RTFEM.md](RTFEM.md)** (`cl_rtfem_enable`); it is **not** what `phys_dmm` implements today.

```bash
phys_spawn_sensor 0 0 32
phys_spawn_slider 0 0 64
phys_spawn_heightfield 0 0 0
phys_spawn_fluid 0 0 96 64
phys_debug
phys_status
```

## Map entities

See [EDITOR_BRIDGE.md](EDITOR_BRIDGE.md) — `misc_phys_box|sphere|static|sensor|slider|ragdoll|dmm`, plus `func_destructible`.

- `misc_phys_ragdoll` → Soft Step + ProcAnim + motor (live Euphoria; `dead 1` for death pose; optional `model` `.rag`)
- `misc_phys_dmm` / `func_destructible` → Soft Step DMM proxy + Voronoi fracture pattern

## Scripting

- **QVM traps:** `G_PHYS_CREATEBODY` … `G_PHYS_RAYCAST`, `G_PHYS_PMOVE_CORRECT`, character traps
- **Lua `Engine.Physics`:** table-driven `createBody({...})` or shorthand bodies, sensors, constraints (incl. filter/parallel/cone), rayCast(+filter), convexSweep, overlapSphere/overlapBox, getContacts, attachShape, setFilter, joint spring/limits/steering/targets, pollEvent (incl. `ragdoll`/`bone`), setFriction / setRestitution / setGravity, validateReplay, stats, and Box3D tuning (`setSleepThreshold`, `setContactTuning`, `setMaxLinearSpeed`, `enableSpeculative`, `setDebugDrawFlags`)
- **Euphoria Lua:** `createRagdoll` (death), `spawnBoundAlive`, `forceAnimState`, `hitRagdoll`
- **DMM Lua:** `createDmm`, `fractureDmm`, `dmmStatus`
- **Soft Step AAA Lua:** `getClosestPoint`, `sphereTOI`, `setContinuous`, `setSleepEnabled`, `setSleepThreshold`, `setHingeTarget`, `setSliderTarget`, `setDistanceLength`, `rebuildTree`, `replayOpen`/`replayStep`/`replaySeek`/`replayClose`/`replayStatus`, `setWheelSuspension`/`setWheelSpin`, `setMotorVelocities`, `setSphericalTarget`, `setBodyDamping`/`setBodyType`, `applyWind`, `explode`, `rayCastAll`, `setGravityScale`/`setMotionLocks`/`setVelocity`/`applyForce`/`applyTorque`/`destroyConstraint`/`setTargetTransform`
- **Lua `Engine.Character`:** `create`, `move`, `destroy`, `getState` (CastMover)

## Ragdoll bind

Optional sidecar `models/<model>.rag`:

```
scale 1.0
bone 0 tag_torso 8 20 0 0 0 -1
bone 1 tag_head 5 10 0 0 28 0
```

Then drive poses with `Phys_RagdollSetBoneAnimTarget` / `Phys_RagdollApplyMd3Frame` + `Phys_RagdollBlendToAnimation`. Death path: `Phys_RagdollSpawnBound`, trap `G_PHYS_CREATERAGDOLL` / `CG_PHYS_CREATERAGDOLL`, Lua `Engine.Physics.createRagdoll`, console `phys_spawn_ragdoll_bind`. Without a bind, Soft Step uses the procedural 11-bone layout.

## Backend switch

| Backend | CMake / script |
|---------|----------------|
| **Box3D (default)** | `-DIDTECH3_PHYSICS_BACKEND=box3d` / `./scripts/compile_engine.sh vulkan` |
| Bullet | `… bullet` (`IDTECH3_PHYSICS_BACKEND=bullet`) |
| Jolt | `… jolt` (`IDTECH3_PHYSICS_BACKEND=jolt`) |
| None | `… no-physics` |

## Cvars

| Cvar | Default | Role |
|------|---------|------|
| `phys_enabled` | 1 | Soft Step world |
| `sv_physSpawn` | 1 | Map `misc_phys_*` spawn |
| `phys_pmove` | 0 | CastMover Pmove bridge |
| `phys_stepHeight` | 18 | CastMover stair step (Soft Step) |
| `phys_bspGridStep` | 24 | BSP height-grid denseness |
| `phys_record` | 0 | Allow Soft Step recording |
| `phys_hitThreshold` | 25 | Soft Step hit-event approach speed |
| `phys_motor` | 1 | Euphoria-like motor layer |
| `phys_dmm_enabled` | 1 | Soft Step DMM companion |
| `phys_dmm_resolution` | 8 | DMM stress grid resolution |
| `phys_dmm_fracture` | 1 | Spawn Soft Step debris on break |
| `phys_debugDraw` | 0 | Wireframe |
| `phys_debugContacts` | 0 | Draw Soft Step contacts when debug on |
| `phys_contactHertz` | 0 | Soft Step contact tuning (0 = default) |
| `phys_contactDamping` | 0.7 | With `phys_contactHertz` |
| `phys_maxLinearSpeed` | 0 | Soft Step max linear speed clamp (0 = off) |
| `phys_speculative` | 1 | Soft Step speculative contacts |

Console: `phys_spawn_ragdoll`, `phys_hit_ragdoll`, `phys_spawn_dmm`, `phys_hit_dmm`, `phys_record_*`, `phys_replay` (hash), `phys_replay_open|step|seek|close`, `phys_closest`, `phys_set_continuous`, `phys_debug_flags`, `phys_rebuild_tree`, `phys_explode`, `phys_ray_all`, `phys_wind`, `phys_set_friction`, `phys_set_restitution`, `phys_set_filter`, `phys_dump`.
