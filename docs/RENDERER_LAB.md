# Renderer Reference Laboratory

**Status:** Foundation Consolidation — deterministic validation scenes, camera bookmarks, capture list.  
**Source:** `vk_reference_lab.c` · **Overlay cfg:** `config/vulkan_overlay_raster_ultra_1_11_reference_lab.cfg`

---

## Ownership

| Asset | Owner |
|-------|-------|
| Scene enum (`VK_REFLAB_SCENE_*`) | `vk_reference_lab.h` |
| Bookmarks / camera poses | Per-scene tables in `vk_reference_lab.c` |
| Decomposition modes | `VK_REFLAB_DECOMP_*` (direct, indirect, shadows, …) |
| Capture determinism | Lab freezes exposure, disables TAA jitter when active |

Lab does **not** add rendering techniques — pins cvars and documents expected captures.

---

## Data flow

```text
exec modern_raster_reference.cfg
exec vulkan_overlay_raster_ultra_1_11_reference_lab.cfg
r_referenceLab 1 → vk_reference_lab_begin_frame each frame
  → scene / decomp / reference mode from cvars
  → bookmark camera override
Captures: renderer_validate_frame, shading_compare, path status, optional screenshots
```

Foundation Consolidation adds cross-module capture checklist (frame contract, BRDF, shadows, HDR).

---

## Buffer formats

Lab uses production buffers — no separate lab formats. Reference modes:

| Mode | Purpose |
|------|---------|
| `VK_REFLAB_REF_SPATIAL_2X/4X/8X` | SSAA reference |
| `VK_REFLAB_REF_MATERIAL` | Material isolation |
| `VK_REFLAB_REF_LIGHTING` | Lighting decomposition |
| `VK_REFLAB_REF_PRESENTATION` | HDR / tonemap chain |

---

## Lifecycle

1. `vk_reference_lab_init()` — register cvars + commands.
2. User sets `r_referenceLab 1`, `r_referenceLabScene N`.
3. `vk_reference_lab_begin_frame()` — apply deterministic overrides.
4. `reference_lab_status` / `reference_lab_scenes` — introspection.
5. `vk_reference_lab_shutdown()` on renderer teardown.

---

## Fallback behavior

- `r_referenceLab 0` — lab inactive; normal gameplay cvars.
- Missing external BSP pack — scene falls back to demo cfg hints (logged).
- RTX / hybrid off in overlay cfg by default (`r_hybrid1 0`).

---

## Debug commands

| Command | Role |
|---------|------|
| `reference_lab_status` | Active, scene, decomp, reference mode, bookmark |
| `reference_lab_scenes` | List all scenes + bookmark counts |
| `renderer_validate_frame` | Post-scene validation |
| `renderer_capture_frame_contract` | Contract snapshot for logs |

---

## Lab sections (scenes)

| Index | Scene | Foundation hook |
|-------|-------|-----------------|
| 0 | Material spheres | BRDF parity |
| 1 | Rough metal sweep | Specular AA |
| 5–6 | Tangent / direct lights | Normal + lighting |
| 8 | Shadows | Shadow contract |
| 9 | GI | Indirect lighting |
| 10 | Reflections | Reflection hierarchy |
| 22 | HDR presentation | HDR pipeline |
| 23 | Weapon / UI | Architecture B composite |

See `VK_REFLAB_SCENE_COUNT` in header for full list (23 scenes).

---

## Camera sequence

Each scene defines up to **three bookmarks** (`vk_reference_lab_bookmark`). Typical soak:

```text
reference_lab_scenes
set r_referenceLabScene <n>
cycle r_referenceLabBookmark 0 1 2
renderer_validate_frame
```

Surf-speed demo: `exec demo_reference_lab_surf_speed.cfg` (when `VK_REFLAB_SCENE_SURF_SPEED` present).

---

## Capture list (Foundation Consolidation)

Run after each bookmark or scene change:

1. `renderer_validate_frame`
2. `renderer_frame_status` (or `renderer_capture_frame_contract`)
3. `shading_compare_status` / `hdr_pipeline_status`
4. `gpu_scene_status` (when GPU path on)
5. `shadow_status` / `reflection_hierarchy_status` / `indirect_light_status`
6. `render_path_status verbose`
7. Optional: `renderer_capture_black_frame` on failure

---

## Performance cost

Lab overhead: cvar pinning + bookmark logic — **negligible**. Spatial reference modes multiply shading cost (2×–8× SSAA).

---

## Known limitations

- External `rtest_*.bsp` pack optional — not in engine repo.
- Not all 23 scenes have automated CI captures yet.
- `VK_REFLAB_SCENE_SURF_SPEED` may be branch-specific enum extension.

---

## Next milestone hooks

- `test_renderer_lab_capture.sh` static wiring gate.
- Scripted capture matrix in `scripts/raster_ultra_lab/`.
- Auto-run Foundation Consolidation master test before lab soak.

Regression: `tests/scripts/test_renderer_lab_capture.sh` · `tests/scripts/test_foundation_consolidation.sh`
