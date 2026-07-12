# Physics Systems

## Architecture

**Box3D Soft Step** ([timfox/idTech3-box3d](https://github.com/timfox/idTech3-box3d)) is the default **rigid substrate** (`third_party/box3d`). Companion solvers share that world through `Phys_*`. Bullet remains an optional alternate.

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

### Optional / MED–LOW (not blockers)

| API | Notes |
|-----|-------|
| Custom filter / pre-solve callbacks | Soft Step callbacks not wired yet |
| Full FEM soft bodies | Not in Box3D — keep XPBD/DMM companions ([timfox/idTech3-box3d](https://github.com/timfox/idTech3-box3d) Soft Step is rigid) |
| Interactive RecPlayer seek/step | Hash validate only today |
| Open-world sector mesh stream | Follow-on to BSP grid bake |
| Distance / TOI queries | Soft Step available; not exposed yet |

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

```bash
phys_spawn_sensor 0 0 32
phys_spawn_slider 0 0 64
phys_spawn_heightfield 0 0 0
phys_spawn_fluid 0 0 96 64
phys_debug
phys_status
```

## Map entities

See [EDITOR_BRIDGE.md](EDITOR_BRIDGE.md) — `misc_phys_box|sphere|static|sensor|slider|ragdoll`.

## Scripting

- **QVM traps:** `G_PHYS_CREATEBODY` … `G_PHYS_RAYCAST`, `G_PHYS_PMOVE_CORRECT`, character traps
- **Lua `Engine.Physics`:** bodies, sensors, constraints (incl. filter/parallel/cone), rayCast(+filter), convexSweep, overlapSphere, getContacts, attachShape, setFilter, joint spring/limits/steering, pollEvent, setFriction / setRestitution, validateReplay

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

Console: `phys_spawn_ragdoll`, `phys_hit_ragdoll`, `phys_spawn_dmm`, `phys_hit_dmm`, `phys_record_*`, `phys_replay`, `phys_set_friction`, `phys_set_restitution`, `phys_set_filter`, `phys_dump`.
