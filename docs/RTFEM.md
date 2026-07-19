# Real-Time Deformation and Fracture (SCA 2009)

Parker & O’Brien, *Real-Time Deformation and Fracture in a Game Environment*, Eurographics/ACM SIGGRAPH Symposium on Computer Animation 2009 (Pixelux **DMM** / *Star Wars: The Force Unleashed*).

This module is a **research scaffold**: paper pipeline stages, calibrated constants, and an honest gap map vs the engine’s existing DMM. It does **not** implement corotational tet FEM.

## Toggle

| Cvar | Default | Purpose |
|------|---------|---------|
| `cl_rtfem_enable` | `0` | Gate console commands |

## Build

```bash
./scripts/compile_engine.sh vulkan full
ctest -R unit_rtfem -V
./tests/scripts/test_rtfem.sh build-vk-Release
```

Requires **`USE_RESEARCH_EXTENSIONS=ON`**.

## Engine DMM ≠ SCA09 FEM

Shipping destructibles ([PHYSICS.md](PHYSICS.md)): Soft Step **rigid proxy** + stress grid → rigid debris. Soft deformables: XPBD cloth/softblob. That is **not** Parker/O’Brien corotational tet FEM + CG + tet-boundary fracture.

## Model mapping

| Paper | Engine API |
|-------|------------|
| Pipeline §3–7 | `RtFem_GetStage` (11 stages) |
| Volume &lt;6% → QR | `RtFem_InvertVolumeThreshold` |
| CG rel err 0.001 | `RtFem_CgRelError` |
| Large island ≥60 and &gt;¼ live | `RtFem_LargeIslandHeuristic` |
| Fracture min 3 face-connected tets | `RtFem_FractureMinTets` |
| Feature vs engine | `RtFem_GetGap` |

## Console

```
set cl_rtfem_enable 1
rtfem_paper
rtfem_pipeline
rtfem_constants
rtfem_gaps
rtfem_advice fracture
rtfem_status
```

## Demo

```
exec demo_rtfem.cfg
```

## Follow-up

`modules/physics/phys_rtfem.c` POST-step tet FEM + CG + fracture islands + deform-mesh GPU skinning — separate epic.

## References

- Parker & O’Brien, SCA 2009
- Engine physics: [PHYSICS.md](PHYSICS.md)
