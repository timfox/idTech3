# RTX hit-shader UV / bindless texturing (D2 design)

**Status:** design only (Jul 2026). Pack-time albedo thumbs + SSBOs remain the shipping chocolate path.

## Problem

Today Hybrid1 / Surfel / pathtrace closest-hit shaders read **one RGB per primitive** from `WorldAlbedoSSBO` / `EntityAlbedoSSBO`. That color comes from:

- BSP vertex / face colors, or
- shader `avgColor`, or
- **UV-centroid** sample of an 8×8 diffuse thumb (`R_EnsureImageThumb`, `vk_rtx_material_*`).

This is good for chocolate RT but is **not** true texturing:

- No barycentric UV at the hit point
- No per-texel filtering or mip selection
- Glint NDF on Hybrid1 uses a **screen-UV jacobian proxy** (`hybrid1_spec.rgen`), not material UV
- Multi-layer `r_materialBlend` / anim / video stages are out of scope for pack walk

## Goals (D2)

1. Sample **material diffuse (and later ORM)** at hit UV in closest-hit / continuation shaders.
2. Keep **fallback** to per-prim SSBO albedo when bindless slot is missing or out of budget.
3. No gameplay API changes; opt-in via existing RT material cvars until a dedicated latch is needed.
4. Reuse raster texture assets (`image_t` / `tr.images[]`) — no second asset pipeline.

## Non-goals (first milestone)

- Full PBR in hit shaders (normal maps, clearcoat, material blend stacks)
- Lightmap sampling in RT (keep bake via `r_rtxWorldAlbedoMode 1` on pack path)
- Descriptor indexing on every consumer (Hybrid1, Surfel, pathtrace, rtx_demo) in one PR

## Current descriptor budget (Vulkan renderer)

| Resource | Count / notes |
|----------|----------------|
| `VK_DESC_COUNT` | **20** sets when `USE_VK_PBR` (uniform, 3×color, fog, BRDF LUT, normal, physical, env, prefilter, …, Forward+ SSBO, **8-layer blend arrays**) |
| `vk.maxBoundDescriptorSets` | Device limit (typically ≥ 8; PBR requires **≥ 10**) |
| `MAX_DRAWIMAGES` | **32768** registered `image_t` |
| Per-texture descriptor | Each `image_t` has `descriptor` (combined image+sampler) in the **global sampler pool** |
| Hybrid1 RT set (binding 0) | AS + storage images + UBO + depth/normal/material + sky + BRDF LUT + **4× SSBO** (world/entity albedo+normal) — **no spare binding** for a large texture array |

**Implication:** we cannot bind “one descriptor per image” on the RT set. D2 needs either **bindless indexing** or a **small atlas + indirection table**.

## Proposed architecture (phased)

### Phase A — Indirection SSBO (minimal bindless)

Pack time (world + entity BLAS rebuild), extend primitive records:

```c
struct RtxPrimMaterial {
    uint32_t textureIndex;  // index into bindless table, 0xFFFFFFFF = use SSBO RGB fallback
    uint16_t uvSet;         // 0 = texcoord0 (future: lightmap set)
    uint16_t flags;         // bit0: hasThumbFallback, bit1: sRGB, …
};
```

Runtime:

1. **Texture table** — dense `uint32_t` array of `image_t*` indices (or stable handles), rebuilt on `vid_restart` / image purge.
2. **Bindless descriptor** — one `VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER` array (or `STORAGE` + manual filter) sized to `min(activeImages, R_TX_BINDLESS_CAP)`.
3. **Extension gate** — require `VK_EXT_descriptor_indexing` + `shaderSampledImageArrayNonUniformIndexing` (and optionally `runtimeDescriptorArray`) when `R_TX_BINDLESS_CAP > 0`.
4. **Hit shader** — `nonuniformEXT` index from `textureIndex`; sample with hit barycentric UV (from built-in barycentrics or reconstructed from vertex attributes in AS build).

**Fallback chain:** bindless sample → SSBO `rgb[]` → G-buffer reprojection (existing Hybrid1 path).

### Phase B — Atlas fallback (no indexing extension)

For GPUs without robust indexing:

- Pack top-N map textures into a **single 2D array or atlas** (similar spirit to lightmap atlases).
- SSBO stores `{ atlasLayer, uvBounds[4] }` per primitive.
- Lower quality, bounded memory, no 32k descriptors.

Prefer Phase A on RTX-class hardware; keep Phase B as `r_rtxBindlessMode 2` fallback.

### Phase C — UV + glint

Once material UV is available in `hybrid1_spec.rgen` / hit stages:

- Replace screen `uvJac` with `dFdx/dFdy` of **material UV** (or analytic footprint from ray differentials if added later).
- Wire `glint_ndf.glsl` with the same `GlintParams` UBO fields already pushed via `params4`/`params5`.

## Descriptor layout sketch (Hybrid1 RT set)

Add binding **15** (new) on RT pipeline only:

| Binding | Type | Stages | Purpose |
|---------|------|--------|---------|
| 15 | `COMBINED_IMAGE_SAMPLER` array `R_TX_BINDLESS_CAP` | closest-hit, raygen (spec) | diffuse bindless |
| 16 | `STORAGE_BUFFER` | closest-hit | `RtxPrimMaterial` per primitive (world + entity ranges) |

Keep bindings 9–14 SSBO albedo/normal for fallback. **Do not remove** pack-time thumbs until Phase A is stable in CI.

Suggested caps (tunable cvars):

| Cvar | Default | Meaning |
|------|---------|---------|
| `r_rtxBindless` | 0 | Master latch (off until Phase A lands) |
| `r_rtxBindlessCap` | 4096 | Max textures in bindless array |
| `r_rtxBindlessMode` | 0 | 0=off, 1=indexing, 2=atlas fallback |

## CPU work items (implementation checklist)

1. `vk_rtx_material.c` — emit `textureIndex` + UV set when packing prims (reuse `vk_rtx_material_diffuse_image`).
2. `vk_rtx_bindless.c` (new) — build table from `tr.images`, update after map load / image registration.
3. `vk_hybrid1.c` / `vk_surfel_gi.c` / hit `.glsl` — new bindings + `GL_EXT_nonuniform_qualifier`.
4. AS vertex buffers — ensure UV attribute available for barycentric interpolation (world faces already have ST coords; entities have texcoord0).
5. `rtx_status` — `bindless=textures:N cap:M mode:K`.
6. Tests — `test_vulkan_rtx.sh` wiring + software RT off fallback.

## Risks

| Risk | Mitigation |
|------|------------|
| Descriptor indexing not on all Vulkan RT GPUs | Phase B atlas; keep SSBO fallback |
| 32k images vs 4k cap | LRU by shader touch; pack only textures referenced by visible BLAS |
| std140 / set count pressure | RT-only set 0; do not expand main PBR `VK_DESC_COUNT` |
| Validation noise | Feature gate + single bindless init log line |
| Animated textures | v1: static image at pack; v2: `textureIndex` invalidation on frame change |

## Playtest note (Jul 2026)

Short automated client pass on OpenArena `oa_dm1` with deferred + Hybrid1 latched originally **SIGSEGV'd during map load** after deferred lighting pipeline init (reproduced with `r_hybrid1 0`).

- **Fixed (Jul 2026):** the deferred post-bloom composite/debug graphics pipelines set `viewportCount`/`scissorCount` = 1 with NULL `pViewports`/`pScissors` and no `pDynamicState`, but the draw path sets viewport/scissor dynamically. Without validation layers the NVIDIA driver NULL-dereferenced during `vkCreateGraphicsPipelines`. Both pipelines now declare `VK_DYNAMIC_STATE_VIEWPORT` / `VK_DYNAMIC_STATE_SCISSOR` (`vk_deferred_gbuffer.c`). Deferred-only map load now completes cleanly.
- **Fixed (Jul 2026) RTX demo DEVICE_LOST (world path):** several AS lifetime bugs in `vk_rtx.c`:
  1. `r_rtxEntities 0` destroyed the TLAS instance buffer before the build that still referenced it.
  2. AS builds reused `vk.tess[0]` (often the active frame CB) — now dedicated `vk_begin/end_command_buffer`.
  3. Scratch addresses were not aligned to `minAccelerationStructureScratchOffsetAlignment`.
  4. World BLAS gated on `RDF_NOWORLDMODEL` / mid-pass rebuild, so the first post-load pack never stuck or destroyed TLAS while recording — world rebuild now runs in `vk_rtx_frame_begin` from `tr.world` name only.
  - **Verified:** `r_rtxDemo 1` + `r_rtxEntities 0` loads `oa_dm1`, packs thousands of world tris, `rtx_status` shows `geo_is_world=1`, clean quit.
- **Fixed (Jul 2026) Hybrid1 DEVICE_LOST:** dummy entity SSBOs (13/14); safe specular lobe (no Heitz VNDF NaNs); diffuse mild-cone sampling + `vec4` payload; A-trous first-frame layout + ping-pong barriers. **Verified:** `r_hybrid1Quality` 1 and 3 on `oa_dm1` with deferred + `r_rtxEntities 0` clean quit.
- **Fixed (Jul 2026) entity TLAS mid-frame destroy:** once-per-frame entity rebuild; retire old TLAS/entity BLAS instead of destroying under an open CB; flush retired after queue idle (world rebuild) / next entity refresh. **Verified:** Hybrid1 quality 1/3 + `r_rtxEntities 1` on `oa_dm1` (entity BLAS UPDATE, 2 TLAS instances) clean quit.
- **Next:** D2 Phase A (hit-shader UV / bindless) after visual Hybrid1 QA with entities.

## References

- Pack helpers: `renderers/vulkan/extensions/rtx/vk_rtx_material.c`
- Hit SSBOs: `renderers/vulkan/shaders/glsl/hybrid1/hybrid1_hit.glsl`
- Glint: `renderers/vulkan/shaders/glsl/glint_ndf.glsl`, `r_hybrid1_glint`
- Debt board: D2 in rtx-tech-debt canvas
