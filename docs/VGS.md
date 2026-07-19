# Gram-Schmidt Voxel Constraints (MIG 2024)

McGraw, *Gram-Schmidt Voxel Constraints for Real-Time Destructible Soft Bodies*, ACM MIG ’24 — [DOI 10.1145/3677388.3696322](https://doi.org/10.1145/3677388.3696322).

This module is a **research scaffold**: PBD pipeline stages, paper constants, face-break anisotropy notes, and a **CPU port of Algorithm 1** (`Vgs_ProjectVoxel`) for unit validation. It does **not** replace engine softblob or ship GPU compute / mesh shaders.

## Toggle

| Cvar | Default | Purpose |
|------|---------|---------|
| `cl_vgs_enable` | `0` | Gate console commands |

## Build

```bash
./scripts/compile_engine.sh vulkan full
ctest -R unit_vgs -V
./tests/scripts/test_vgs.sh build-vk-Release
```

Requires **`USE_RESEARCH_EXTENSIONS=ON`**.

## Honesty vs softblob

Shipping deformables ([PHYSICS.md](PHYSICS.md)): XPBD **distance-lattice** softblob (`phys_softblob`) and cloth. That is **not** per-voxel Gram-Schmidt + breakable face-to-face constraints from MIG ’24.

| Paper | Engine today |
|-------|----------------|
| VGS parallelepiped (Alg. 1) | CPU `Vgs_ProjectVoxel` only (tests / scaffold) |
| Face strain partitions (3) | Absent |
| Soft companions | `softblob` / `xpbd_cloth` POST solvers |

## Algorithm 1 (summary)

For each of `vgs_it` iterations on 8 non-shared voxel corners:

1. Centroid \(c\)
2. Average the four edges along each axis → \(v_0,v_1,v_2\)
3. Relaxed Gram-Schmidt with \(\alpha\) (default **0.5**)
4. Edge-length blend with \(\beta\) toward \(r\) vs \(\|v_i\|/2\)
5. Volume scale \(\tfrac12(V_0/V)^{1/3}\)
6. Reconstruct corners \(c\pm u_0\pm u_1\pm u_2\) (respect static \(w_i=0\))

## Console

```
set cl_vgs_enable 1
vgs_paper
vgs_pipeline
vgs_constants
vgs_gaps
vgs_advice fracture
vgs_status
```

Advice keys: `soft` | `fracture` | `aniso` | `limit`.

## Demo

```
exec demo_vgs.cfg
```

## Follow-up

`modules/physics/phys_vgs.c` POST-step voxel PBD (VGS + face partitions) + optional mesh embed — separate epic. Out of scope for v1: mesh voxelization, GLSL compute loop, MLS-MPM harness.

## Related

- Soft deformables: [PHYSICS.md](PHYSICS.md)
- Tet FEM scaffold (alternative): [RTFEM.md](RTFEM.md)

## References

- McGraw, MIG ’24, DOI 10.1145/3677388.3696322
