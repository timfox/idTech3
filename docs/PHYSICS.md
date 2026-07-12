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
| Profile counters (`phys_status`) | Done |
| Recording (`phys_record` + `Phys_StartRecording`) | Done |

### Optional / MED–LOW (not blockers)

| API | Notes |
|-----|-------|
| Custom query filter callbacks | Default filter only today |
| Contact begin/end (non-sensor) | Hit events only |
| Full FEM soft bodies | Not in Box3D — keep XPBD/DMM companions |
| Open-world sector mesh stream | Follow-on to BSP grid bake |

## Multi-solver companions

| Name | Phase | Console |
|------|-------|---------|
| `shadows` / `volumes` / `procanim` / `motors` | PRE | shadow / buoyancy / ragdoll |
| `xpbd_cloth` / `particles` / `softblob` / `fluid` | POST | `phys_spawn_*` |

```bash
phys_solvers
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
- **Lua `Engine.Physics`:** bodies, sensors, constraints, rayCast, moverStep, pmoveCorrect, heightfield, backend

## Ragdoll bind

Optional sidecar `models/<model>.rag`:

```
scale 1.0
bone 0 tag_torso 8 20 0 0 0 -1
bone 1 tag_head 5 10 0 0 28 0
```

Then drive poses with `Phys_RagdollSetBoneAnimTarget` + `Phys_RagdollBlendToAnimation`. Without a bind, Soft Step uses the procedural 11-bone layout.

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
| `phys_bspGridStep` | 24 | BSP height-grid denseness |
| `phys_record` | 0 | Allow Soft Step recording |
| `phys_debugDraw` | 0 | Wireframe |
