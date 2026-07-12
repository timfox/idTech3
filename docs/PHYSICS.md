# Physics Systems

## Architecture

**Box3D Soft Step** ([timfox/idTech3-box3d](https://github.com/timfox/idTech3-box3d)) is the default **rigid substrate**. A **multi-solver registry** (`phys_solvers`) runs companions **before and after** Soft Step, sharing one world through `Phys_*` (ray / overlap / impulse / force). Bullet remains an optional rigid alternate.

```txt
Phys_StepSimulation
  ├─ PhysSolvers_PreStep
  │     ├─ shadows   (kinematic sync)
  │     ├─ volumes   (buoyancy / drag forces)
  │     ├─ procanim  (ragdoll state)
  │     └─ motors    (active PD torques)
  ├─ Soft Step (Box3D)          ← primary rigid solver
  ├─ contact events
  └─ PhysSolvers_PostStep
        ├─ softstep   (status → body count)
        ├─ xpbd_cloth
        ├─ particles
        ├─ softblob
        └─ fluid      (SPH-ish + rigid couple)
```

> Box3D does **not** ship FEM soft bodies or CFD. Soft / fluid behavior comes from companion solvers that **use** Soft Step collision.

| Layer | Module | Role |
|-------|--------|------|
| Rigid substrate | `phys_box3d_impl.c` | Soft Step bodies, joints, CastMover, mesh/compound |
| Solver registry | `phys_solvers` | Register / enable / Pre+Post tick / debug |
| PRE layers | props / volumes / ProcAnim / motors | Force Soft Step to see game + character control |
| XPBD cloth | `phys_cloth` | Soft sheets via `Phys_RayCast` |
| Particles | `phys_particles` | Debris Verlet + bounce |
| Soft blob | `phys_softblob` | Lattice jelly |
| Fluid | `phys_fluid` | SPH-ish blob + Soft Step couple |
| Middleware | `phys_middleware` | Demos, console, event dispatch |

## Solvers

| Name | Phase | Cvar | Console |
|------|-------|------|---------|
| `shadows` | PRE | — | `phys_spawn_shadow` |
| `volumes` | PRE | — | `phys_spawn_buoyancy` |
| `procanim` / `motors` | PRE | `phys_motor` | `phys_spawn_ragdoll` |
| `softstep` | (primary) | — | — |
| `xpbd_cloth` | POST | — | `phys_spawn_cloth` |
| `particles` | POST | `phys_particles` | `phys_spawn_particles` |
| `softblob` | POST | `phys_softblob` | `phys_spawn_softblob` |
| `fluid` | POST | `phys_fluid` | `phys_spawn_fluid` |

```bash
phys_solvers
phys_solvers fluid off
phys_spawn_fluid 0 0 96 96
phys_spawn_softblob 0 0 80
phys_debug
phys_status
```

## Capability matrix

| Feature | Status |
|---------|--------|
| Rigid Soft Step + workers/sleep/CCD | Done |
| Multi-solver Pre/Post registry | Done |
| PRE motors / volumes / shadows | Done |
| Cloth / particles / softblob / fluid | Done |
| Distance-joint ropes | Done |
| FEM volumetric soft / production CFD | Not in Box3D |
| Map entity auto-spawn / MD3 ragdoll bind | Open |

## Backend switch

| Backend | CMake / script |
|---------|----------------|
| **Box3D (default)** | `-DIDTECH3_PHYSICS_BACKEND=box3d` / `./scripts/compile_engine.sh vulkan` |
| Bullet | `… bullet` |
| None | `… no-physics` |

## Dual motion + companions

| Mode | API |
|------|-----|
| Free rigid | `PhysProp_CreateDynamic` |
| Shadow kinematic | `PhysProp_CreateShadow` |
| Cloth / particles / softblob / fluid | `phys_spawn_*` |
| Rope | `PHYS_CONSTRAINT_DISTANCE` / `phys_spawn_rope` |

**Cvars:** `phys_enabled`, `phys_workers`, `phys_sleep`, `phys_ccd`, `phys_particles`, `phys_softblob`, `phys_fluid`, `phys_debugDraw`, …

**Commands:** `phys_status`, `phys_solvers`, `phys_spawn_*`, `phys_debug`

## Roadmap

1. **Done:** Soft Step + multi-solver (PRE motors/volumes, POST cloth/particles/softblob/fluid)
2. **Open:** MD3 ragdoll bind, map entity spawn, prefracture
