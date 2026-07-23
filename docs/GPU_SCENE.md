# GPU Scene

**Status:** GPU-Driven Visibility Milestone 1 — **Phase 1 certified**: persistent `gpuSceneObject_t` + object SSBO.  
**Related:** [GPU_DRIVEN_RENDERING.md](GPU_DRIVEN_RENDERING.md) · [GPU_VISIBILITY.md](GPU_VISIBILITY.md) · [GPU_INDIRECT_DRAWS.md](GPU_INDIRECT_DRAWS.md) · `vk_gpu_scene.c`

Canonical **`gpuSceneObject_t`** (`GpuSceneObject` / `vkGpuSceneInstance_t` aliases) is the sole authoritative scene record. No CPU pointers in GPU records. Classic BSP remains authoritative when `r_gpuSceneWorldType 0` or stream metadata is absent. CPU direct-draw (`r_gpuDriven 0`) remains the correctness reference.

---

## Shared consumers (one database)

| Consumer | Path / list |
|----------|-------------|
| Deferred opaque | `VK_GPU_PATH_DEFERRED` → `VK_GPU_DRAW_LIST_DEFERRED_OPAQUE` |
| Forward+ opaque fallback | `VK_GPU_PATH_FORWARD_FALLBACK` |
| Alpha-tested | `VK_GPU_PATH_ALPHA_TEST` |
| WBOIT transparency | `VK_GPU_PATH_TRANSPARENT` |
| Shadows | `VK_GPU_PATH_SHADOW` |
| Depth prepass | `VK_GPU_PATH_DEPTH_PREPASS` |
| Object-ID / debug | `VK_GPU_PATH_OBJECT_ID` → debug list |
| Temporal velocity | `VK_GPU_PATH_VELOCITY` |
| Weapon (independent ownership) | `VK_GPU_PATH_WEAPON` |
| Reflection / probe assignment | later (`reflectionProbeIndex` / `irradianceProbeIndex` reserved) |

Material routing and render-path ownership are unchanged — the scene DB only classifies and submits.

---

## Ownership

| Buffer / record | Owner module | Consumers |
|-----------------|--------------|-----------|
| Object records (`gpuSceneObject_t`) | `vk_gpu_scene.c` | Cull, indirect pack, meshlets, debug, future GPU cull |
| Persistent object SSBO | `vk_gpu_scene.c` | Shared GPU readers (Phase 1 host-visible upload) |
| Mesh records | `vk_gpu_scene.c` | Indirect draw, LOD |
| Indirect command buffer | `vk_gpu_scene.c` | MDI / meshlet draws (when enabled) |
| Materials / lights / clusters | Forward+ SSBOs | Shading (separate contracts) |
| Shadow atlas | `vshadow` / CSM | Shadow sampling |
| Probes | `vk_indirect_light.c` | Indirect diffuse |

---

## Data flow

```text
Register mesh/instance (map load / entity spawn / pilot)
  → vk_gpu_scene_register_* (host records)
Begin frame → set prev transforms
  → frustum cull (+ optional Hi-Z)
  → compact visible list
  → pack VkDrawIndexedIndirectCommand (host)
  → upload object SSBO (full table)
  → (optional) merge compatible draws
Draw consumers read indirect + object buffers when r_gpuScene 1
```

---

## Buffer formats

### Preferred GPU contract (`gpuSceneObject_t`)

| Field | Type | Purpose |
|-------|------|---------|
| `currentModel` / `previousModel` | `float[16]` | Motion / velocity |
| `boundsSphere` / `boundsMin` / `boundsMax` | `float[4]` | Cull bounds |
| `objectId` / `objectGeneration` | `uint32` | Temporal identity |
| `meshId` / `materialId` | `uint32` | Draw + shading |
| `surfaceId` / `renderPath` / `renderFlags` / `temporalClass` | `uint32` | Routing |
| `lightmapIndex` / reflection / irradiance / `animationIndex` | `uint32` | Lighting / anim |
| `pipelineKey` / `shadowFlags` / `visibilityFlags` / `instanceDataIndex` | `uint32` | Submit / shadow / instancing |

Host lifecycle helpers (`handle`, `transform[12]`, `mins`/`maxs`/`sphere`, generation stamps, free-list state) follow in the same struct — still no pointers. `sizeof(gpuSceneObject_t) % 16 == 0` (512 bytes).

**Mesh** (`vkGpuSceneMesh_t`): meshlet range, index range, AABB, generation.

**Indirect** (`vkGpuSceneDrawCmd_t`): standard Vulkan indexed indirect layout.

Limits: 4096 instances, 1024 meshes, 8192 indirect commands.

---

## Lifecycle

1. **Cvar register** — `vk_gpu_scene_register_cvars()` (`r_gpuScene`, cull, indirect, world type, `r_gpuDriven`).
2. **Init** — `vk_gpu_scene_init()` creates indirect + object SSBOs when `r_gpuScene 1`.
3. **World load** — `vk_gpu_scene_on_world_load()` bumps generation; classic path preserved.
4. **Per frame** — `begin_frame` → cull/indirect → object SSBO upload → `end_frame`.
5. **Vid restart** — `vk_gpu_scene_on_vid_restart()` rebuilds GPU-side buffers.
6. **Shutdown** — destroy buffers, reset counts.

---

## Fallback behavior

- `r_gpuScene 0` / `r_gpuDriven 0` — classic CPU draw lists; GPU scene inactive.
- Terrain/stream/hybrid requested without metadata → effective world type falls back to classic BSP.
- Object SSBO alloc fail → warning; host records remain authoritative; cull continues.
- Overflow (`VK_GPU_SCENE_REJECT_OVERFLOW`) drops excess; `fallbackObj` → direct path.

---

## Debug commands

| Command / cvar | Role |
|----------------|------|
| `gpu_scene_status` | Active, world type, counts, path buckets, reject telemetry |
| `gpu_scene_layout` | Struct sizes, Phase 1 contract, SSBO readiness |
| `gpu_scene_object_status` | Per-handle identity / path / frames |
| `gpu_draw_status` | Indirect + object buffer readiness |
| `r_gpuSceneDebug` | 0–9 object/material/mesh/temporal/path views |

---

## Performance cost

Host-side cull + indirect pack + full object table memcpy: typically **0.1–0.6 ms** for thousands of instances. Hi-Z pyramid build adds **~0.2–1 ms** (`r_hiZ 1`).

---

## Known limitations

- GPU-driven draw dispatch not default; classic BSP draws remain primary for world surfaces.
- Host frustum + Hi-Z readback (not full GPU occlusion compute).
- Pilots opt-in only — no automatic OpenArena world migration.
- Object SSBO is host-visible (Phase 1); device-local multi-buffer upload comes with GPU cull.

---

## Next milestone hooks (after Phase 1)

- Phase 2: conservative Hi-Z certification + grace policy live metrics
- Phase 3: production pilot IndirectDraw consumer (compare vs direct)
- GPU frustum/occlusion compute after parity proven

Regression: `tests/scripts/test_gpu_scene_layout.sh` · foundation GPU suite
