# GPU-Driven Rendering

**Status:** Foundation Consolidation — opt-in cull → indirect path (Raster Ultra).  
**Related:** [GPU_SCENE.md](GPU_SCENE.md) · [MESHLETS.md](MESHLETS.md) · `vk_gpu_scene.c` · `vk_hiz.c`

**`r_gpuDriven`** (consolidation alias): when `1`, enables `r_gpuScene 1` + cull + indirect pack. Underlying cvars remain granular for safe rollout.

---

## Ownership

| Stage | Module | Output |
|-------|--------|--------|
| PVS / portal | `tr_world.c` | Visible leaf set (CPU, classic) |
| Frustum cull | `vk_gpu_scene.c` | Visible instance handles |
| Hi-Z occlusion | `vk_hiz.c` | Conservative reject mask |
| LOD / HLOD | `vk_gpu_scene.c` | `lodLevel`, hysteresis |
| Indirect pack | `vk_gpu_scene.c` | `VkDrawIndexedIndirectCommand` list |
| Draw dispatch | Meshlets / MDI consumers | GPU or MDI draws |

Classic surface draws via `R_SelectSurfaceRenderPath` remain authoritative for BSP world unless pilot cvars enabled.

---

## Data flow

```text
PVS (BSP) → surface marks on CPU
GPU scene instances → frustum test
  → Hi-Z test (conservative: max depth mip, large-object bypass, visibleAge)
  → LOD select (+ HLOD flag when r_gpuSceneHlod 1)
  → compact visible[] + indirect cmds
  → (compare) classic draw count when r_gpuDrawCompare 1
Stale indirect slots cleared each frame before repack (zero instanceCount on unused tail).
```

Telemetry exported via `gpu_scene_status`, `renderer_perf`, and `r_gpuDrawCompare` counters.

---

## Buffer formats

- **Visible list** — `uint32[VK_GPU_SCENE_MAX_INSTANCES]` host compacted handles.
- **Indirect buffer** — host-visible `vkGpuSceneDrawCmd_t[]`, mirrored to device for MDI.
- **Hi-Z pyramid** — `R32_SFLOAT` mips, conservative `min()` downsample (reversed-Z: larger = nearer, farthest = min). Host readback of a coarse mip (≤64²) enables CPU AABB reject with 1-frame lag.

---

## Lifecycle

1. Latch `r_gpuScene 1` / `r_gpuDriven 1` → requires `vid_restart`.
2. `vk_hiz_init()` when `r_hiZ 1` — pyramid sized to main color extent.
3. Each frame: `vk_hiz_build` → `vk_gpu_scene_cull_and_build_indirect`.
4. Camera cut → Hi-Z bias keeps instances visible (`s_cameraCut`).
5. Shutdown destroys pyramid + indirect buffer.

---

## Fallback behavior

- `r_gpuDriven 0` / `r_gpuScene 0` — full classic draw path; zero indirect commands.
- `r_gpuSceneCull 0` — skip cull; all registered instances visible.
- `r_gpuSceneIndirect 0` — cull only; no indirect buffer update.
- Hi-Z missing / camera cut / large projected AABB → instance stays visible (no one-frame pop).
- `r_gpuDrawCompare 1` — logs classic vs GPU visible counts; does not alter draws.

---

## Debug commands

| Cvar / command | Role |
|----------------|------|
| `r_gpuDriven` | 0 classic (default), 1 GPU cull + indirect |
| `r_gpuScene` | Master GPU scene latch |
| `r_gpuSceneCull` / `r_gpuSceneIndirect` | Stage toggles |
| `r_gpuDrawCompare` | Classic vs GPU cull metrics |
| `r_hiZ`, `r_hiZDebug`, `r_hiZMinVisibleFrames`, `r_hiZLargeObjectPx` | Hi-Z policy |
| `gpu_scene_status`, `hiz_status`, `renderer_perf` | Telemetry |

---

## Performance cost

| Component | Typical cost |
|-----------|--------------|
| Frustum cull (CPU) | 0.05–0.3 ms / 4k instances |
| Hi-Z build (GPU) | 0.2–1.0 ms @ 1080p |
| Indirect pack | <0.1 ms |
| MDI draw savings | Reduces CPU submission; GPU bound unchanged |

---

## Known limitations

- Not a full GPU-driven pipeline — no mesh shader cull compute shipping as default.
- PVS still drives BSP surfaces; GPU path augments props / meshlet pilots.
- `r_gpuDrawCompare` scaffolding — counters may not cover all classic draw classes yet.
- Stale command clearing host-side only; GPU does not auto-zero unused indirect slots without repack.

---

## Next milestone hooks

- Formal `r_gpuDriven` cvar alias in `vk_gpu_scene.c`.
- GPU-side compact + indirect compute (eliminate host readback).
- PVS → GPU instance mark for streamed sectors.
- CI gate: `r_gpuDrawCompare` delta < threshold on reference lab LOD scene.

Regression: `tests/scripts/test_gpu_draw_parity.sh` · `tests/scripts/test_hiz_reversed_z.sh`
