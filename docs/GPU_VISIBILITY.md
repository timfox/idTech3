# GPU Visibility

**Status:** GPU-Driven Visibility Milestone 1 — PVS → frustum → Hi-Z stages (Phase 1 scene DB certified).  
**Related:** [GPU_SCENE.md](GPU_SCENE.md) · [HIZ_OCCLUSION.md](HIZ_OCCLUSION.md) · `vk_gpu_visibility.c`

## Ownership

| Stage | Module | Notes |
|-------|--------|-------|
| VISIBILITY_PVS | `tr_world.c` + candidates | Stage 0; never less conservative than BSP |
| VISIBILITY_FRUSTUM | `vk_gpu_scene.c` / frustum math | Sphere test; AABB later |
| VISIBILITY_HIZ | `vk_hiz.c` | Conservative; grace frames |
| VISIBILITY_LOD | `vk_gpu_scene` HLOD/LOD | Opt-in |
| VISIBILITY_RENDER_PATH | draw lists | Deferred / Forward+ / depth / velocity / etc. |
| VISIBILITY_FINAL | published draws | Exact count only |

## Data flow

```text
BSP/PVS candidates → frustum → Hi-Z → LOD → path bucket → indirect cmds
```

## Buffer formats

Candidate handles: `uint32[8192]`. Rejection counters per stage.

## Lifecycle

`vk_gpu_visibility_begin_frame` clears candidates; cull notes rejects; `gpu_visibility_status`.

## Fallback behavior

`r_gpuOcclusion 0` disables Hi-Z only. `r_gpuSceneCull 0` keeps all registered instances.

## Debug commands

`r_visibilityDebug` 1–6, `r_gpuOcclusionDebug` 1–7, `gpu_visibility_status`, `gpu_visibility_perf`.

## Performance cost

Host frustum+Hi-Z: sub-millisecond for thousands of instances. GPU compute frustum is a later stretch goal.

## Known limitations

- Host Hi-Z readback (1-frame lag), not full GPU occlusion compute.
- PVS still owns BSP surfaces; pilots only for opted-in geometry.

## Next milestone hooks

GPU frustum/occlusion compute + meshlets after parity proven.
