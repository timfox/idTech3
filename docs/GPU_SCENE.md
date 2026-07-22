# GPU Scene

**Status:** Foundation Consolidation — persistent GPU scene records (Raster Ultra 1.6 / 1.14).  
**Related:** [GPU_DRIVEN_RENDERING.md](GPU_DRIVEN_RENDERING.md) · [RENDERER_PATH_OWNERSHIP.md](RENDERER_PATH_OWNERSHIP.md) · `vk_gpu_scene.c`

Canonical **`GpuSceneObject`** schema maps to `vkGpuSceneInstance_t` (instance) + `vkGpuSceneMesh_t` (mesh). No CPU pointers in GPU records.

---

## Ownership

| Buffer / record | Owner module | Consumers |
|-----------------|--------------|-----------|
| Instance records (`GpuSceneObject`) | `vk_gpu_scene.c` | Cull, indirect pack, meshlets, debug |
| Mesh records | `vk_gpu_scene.c` | Indirect draw, LOD |
| Indirect command buffer | `vk_gpu_scene.c` | MDI / meshlet draws (when enabled) |
| Materials / lights / clusters | Forward+ SSBOs | Shading (separate contracts) |
| Shadow atlas | `vshadow` / CSM | Shadow sampling |
| Probes | `vk_indirect_light.c` | Indirect diffuse |

Classic BSP remains authoritative when `r_gpuSceneWorldType 0` or stream metadata absent.

---

## Data flow

```text
Register mesh/instance (map load / entity spawn)
  → vk_gpu_scene_register_* (host records)
Begin frame → set prev transforms
  → frustum cull (+ optional Hi-Z)
  → compact visible list
  → pack VkDrawIndexedIndirectCommand (host)
  → (optional) merge compatible draws
Draw consumers read indirect buffer when r_gpuSceneIndirect 1
```

Shared SSBO layout for materials, lights, clusters documented in [CLUSTERED_LIGHTING.md](CLUSTERED_LIGHTING.md).

---

## Buffer formats

**GpuSceneObject** (`vkGpuSceneInstance_t`):

| Field | Type | Purpose |
|-------|------|---------|
| handle, meshId, materialId, objectId | `uint32` | Identity |
| transform, prevTransform | `float[12]` | 3×4 row-major motion |
| mins, maxs, sphere | `vec3` + radius | Cull bounds |
| lodLevel, lodHysteresis | `uint32` | LOD selection |
| flags | `uint32` | static, HLOD, foliage, dynamic; high bits reserved |
| streamState | enum | resident / loading / fallback / evicted |
| visibleAge, lastReject, generation | `uint32` | Temporal stability |

**Mesh** (`vkGpuSceneMesh_t`): meshlet range, index range, AABB, generation.

**Indirect** (`vkGpuSceneDrawCmd_t`): standard Vulkan indexed indirect layout.

Limits: 4096 instances, 1024 meshes, 8192 indirect commands.

---

## Lifecycle

1. **Cvar register** — `vk_gpu_scene_register_cvars()` (`r_gpuScene`, cull, indirect, world type).
2. **Init** — `vk_gpu_scene_init()` creates indirect buffer when `r_gpuScene 1`.
3. **World load** — `vk_gpu_scene_on_world_load()` bumps generation; classic path preserved.
4. **Per frame** — `begin_frame` → cull/indirect → `end_frame`.
5. **Vid restart** — `vk_gpu_scene_on_vid_restart()` rebuilds GPU-side buffers.
6. **Shutdown** — destroy indirect buffer, reset counts.

---

## Fallback behavior

- `r_gpuScene 0` — classic CPU draw lists; GPU scene inactive.
- Terrain/stream/hybrid requested without metadata → effective world type falls back to classic BSP (`vk_gpu_scene_world_fallback_reason`).
- Indirect buffer not mapped → draws skip GPU indirect path silently.
- Overflow (`VK_GPU_SCENE_REJECT_OVERFLOW`) drops excess instances; logged via debug modes.

---

## Debug commands

| Command / cvar | Role |
|----------------|------|
| `gpu_scene_status` | Active, world type, counts, reject telemetry |
| `gpu_scene_layout` | Print struct sizes, field offsets, buffer capacities |
| `r_gpuSceneDebug` | 0 off, 1 bounds, 2 rejection, 3 LOD/HLOD, 4 indirect counts (5–9 reserved: material/light/cluster views) |
| `hiz_status` | Called from `gpu_scene_status` when Hi-Z companion active |

---

## Performance cost

Host-side cull + indirect pack: typically **0.1–0.5 ms** for thousands of instances (no GPU readback). Hi-Z pyramid build adds **~0.2–1 ms** depending on resolution (`r_hiZ 1`). Indirect MDI reduces CPU draw submission when consumers wired.

---

## Known limitations

- GPU-driven draw dispatch not default; classic BSP draws remain primary for world surfaces.
- `r_gpuSceneDebug` currently 0–4; views 5–9 (material atlas, light SSBO, cluster heat) planned.
- No persistent GPU-side instance buffer upload in all configurations — host records authoritative.
- Animated MD3/IQM meshlets skipped in meshlet pilots.

---

## Next milestone hooks

- `gpu_scene_layout` console dump for SSBO parity tests.
- Expand `r_gpuSceneDebug` 5–9 for shared buffer visualization.
- Wire instance `prevTransform` to TAA motion classification.
- Sector stream residency → `streamState` transitions without one-frame pop.

Regression: `tests/scripts/test_gpu_scene_layout.sh`
