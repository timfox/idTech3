# Forward+ render pipeline — audit (Vulkan, 2026)

This document is a **technical audit** of the current **Forward+ scaffolding** in this fork: what runs, what data flows where, synchronization, known limitations, and **risk items** for future work. It complements the narrative in [RENDERER_2026_ARCHITECTURE_PASS.md](RENDERER_2026_ARCHITECTURE_PASS.md).

**Scope:** `r_forwardPlus` (default **1** on Vulkan, **latched**), PBR-only descriptor integration, **dynamic lights** from `backEnd.refdef` (`dlight_t`), **no** replacement of the primary forward lighting path.

---

## 1. Feature summary

| Layer | Responsibility |
|--------|----------------|
| **C / `vk_forward_plus.c`** | CPU packs **staging** (host) light records; **device-local** light SSBO via `vkCmdCopyBuffer` each frame; **tile SSBO** allocation, **param SSBO** (`clipFromWorld` + aux uvec4), compute **pipeline + dispatch**, graphics **descriptor set** (set **18**), tile grid from **`VK_FP_TILE_DIM`** (16 px) and **`vk_get_render_target_width/height`**. |
| **Compute / `forward_plus_tile_cull.comp`** | Per-tile **light index lists** ( **`MAX_PER_TILE` = 8** ), sphere-in-screen projection cull, **`MAX_LIGHTS` = 64** aligned with **`VK_FP_MAX_GPU_LIGHTS`** (classic **`dlightBits`** still **`MAX_DLIGHTS` = 32**). |
| **Fragment / `gen_frag.tmpl`** (PBR) | Optional **debug heatmap** (`r_forwardPlusDebug`), optional **additive experimental shade** (`r_forwardPlusShade` → specialization **`forward_plus_shade_strength`**). Uses **`fp_params.fp_clip_from_world`** and SSBO light + tile data. |
| **Uniform bridge / `tr_shade.c`** | When Forward+ is on, **`pbrForwardPlus.y`** carries **`floatBitsToUint(tess.dlightBits)`** so the fragment path can **skip** culled lights that the surface already received via the classic packed path (first **32** indices). |

**Cvars** (see `tr_init.c`): `r_forwardPlus`, `r_forwardPlusMaxPerTile` (latched **4–8**), `r_forwardPlusDebug`, `r_forwardPlusShade` (pipeline invalidation on change in `vk_frame_submit.c`), `r_forwardPlusSpecularStrength` (default **0.65**), `r_forwardPlusEnergyRenorm` (default **0** — soft primary vs Forward+ mix; `modern_vulkan.cfg` keeps 0), `r_forwardPlusLuminanceSort` (**0/1**, default **1** — tile overload picks brightest lights by RGB sum), `r_forwardPlusDistanceSort` (**0/1**, default **0** — nearest lights when overloaded), `r_forwardPlusDepthCull` (**0/1**, default **0** in code; **`modern_vulkan.cfg` sets 1** — post-opaque tile cull with 5 depth probes per tile).

---

## 2. Frame / command ordering

Within **`RB_DrawSurfs`** (`tr_backend.c`), order is:

1. **`vk_prepare_frame_temporal_state()`**
2. **`vk_forward_plus_ensure_render_resolution()`** — may resize **tile SSBO** if render target dimensions changed (matches FBO / `r_renderScale` via **`vk_get_render_target_*`**).
3. **`vk_forward_plus_update_for_refdef()`** — CPU writes the **staging** buffer with light header + records; clears **tail** when count drops.
4. **`RB_RenderSunShadowMap`**
5. **`RB_BeginDrawingView()`** — begins the **main** render pass.
6. **`vk_forward_plus_upload_refdef()`** — `vkCmdCopyBuffer` staging → **device-local** light SSBO (transfer + shader barriers).
7. **Tile cull dispatch** (see below).
8. **World / entity draws** (opaque; optional OIT opaque pass first).
9. **`vk_forward_plus_dispatch_tile_cull_after_opaque()`** — only when **`r_forwardPlusDepthCull` 1** (same compute shader, **`depthCull`** push constant **1**, depth sampler on **binding 3**).

**Tile cull timing:**

| `r_forwardPlusDepthCull` | When dispatch runs |
|--------------------------|-------------------|
| **0** (default) | Step **7**, before draws (legacy). |
| **1** | Step **9**, after opaque geometry (depth buffer valid for rejection). |

Then PBR draws bind **descriptor set 18** when Forward+ resources are live (`vk_draw_state.c`).

### Mode 3 — Unified Clustered Renderer

When **`r_renderMode 3`** (`vk_unified_clustered_active()`), geometry + deferred are split:

1. Opaque `drawSurfFilter=1` (hybrid handoff: skip Forward+ add; deferred owns dynamics)
2. G-buffer capture + deferred lighting composite
3. Transparent `drawSurfFilter=2` with Forward+ fragment shade (shared tile SSBO)

See [UNIFIED_CLUSTERED_RENDERER.md](UNIFIED_CLUSTERED_RENDERER.md). Mode 3 is the opt-in **Unified Clustered** profile (unified heterogeneous shading / lighting ownership; shared light grid with optional Z-slices via `r_forwardPlusZSlices`); Spine stable default remains Forward+ mode 2.

---

## 3. Data layout (SSBOs)

### 3.1 Light buffer (`binding = 0`)

Packed as **`float`** array in **`vk_forward_plus_update_for_refdef`**:

| Offset (vec4 index) | Content |
|---------------------|---------|
| `data[0]` | **x** = packed light count **n**, **y** = refdef time (ms), **z** = **`max_per_tile`** (effective **4–8**), **w** = debug scale |
| `data[1]` | **x,y** = **`tiles_x`, `tiles_y`**, **z,w** = viewport **width/height** (render target pixels) |
| `data[2 + i*4 …]` | Four **`vec4`** per light **i** (origin+radius, color+linear flag, axis/cone pack, etc.) — mirrors **`dlight_t`** fields |

**PBR shade parity (incremental):** experimental Forward+ shade in **`gen_frag.tmpl`** uses the same **radial** falloff as the classic projected dlight path for **point** lights (`1 - (dist/radius)^2`, matching **`light_frag.tmpl`** / **`VK_SetLightParams`**). **Linear** lights use **`vk_linear_dlight_cone_cosines`** for outer/inner cone cosines (shared with volumetrics). Perpendicular tube falloff uses a **squared** rim term for closer behavior to the point sphere. **`dlight_t.additive`** is packed in the fourth record **`vec4` `.z`** and applies a small brightness boost (legacy **ADD** blend is not identical in PBR, but this reduces “flat” additive props). **`pbrForwardPlus.y`** still carries **`tess.dlightBits`** so indices already handled by the multi-pass projector are **skipped** in Forward+ shade.

**Caps:** up to **`VK_FP_MAX_GPU_LIGHTS` (64)** lights packed from **`backEnd.refdef.dlights`** (matches **`MAX_REAL_DLIGHTS`**). Surface **`tess.dlightBits`** still covers only the first **`MAX_DLIGHTS` (32)** indices for skip/double-count avoidance. Overflow beyond 64 is clamped with a **developer** log.

### 3.2 Tile buffer (`binding = 1`)

Linear array: **`total_tiles × MAX_PER_TILE`** **`uint32`** indices. Unused slots **`0xFFFFFFFF`**. Stride per tile is fixed at **8** slots in the SSBO layout ( **`VK_FP_MAX_PER_TILE`** ); **`r_forwardPlusMaxPerTile`** only limits how many indices the **compute** and **fragment** loops **consume**.

### 3.3 Param buffer (`binding = 2`)

- **`mat4 clipFromWorld`** — **`view × projection_vk`** (same Y-flip convention as MVP path).
- **`uvec4 tiles_xy_viewport`** — redundant with light header in places; used by compute for push/debug consistency.

---

## 4. Compute shader behavior (`forward_plus_tile_cull.comp`)

- **Workgroup:** 64 threads; dispatch **`ceil(totalTiles / 64)`**.
- **Per thread:** one **tileId**; clears **MAX_PER_TILE** slots, gathers all overlapping lights into a thread-local list (**≤ MAX_LIGHTS**), then writes up to **`maxPerTile`** indices.
- **Projection:** `clip = clipFromWorld * vec4(worldPos,1)`; NDC bounds check (with margin on XY); center in **pixels** via **`0.5*(1+ndc)*viewport`**; **screen-radius** heuristic from world radius and **`clip.w`**; **AABB tile overlap** via **`sphere_tile_overlap`** with **`tilePxX/Y = viewport / tileGrid`** (aligned with fragment mapping).

**Ordering / overload:** when a tile has **fewer** overlapping lights than **`maxPerTile`**, output order matches **increasing light index** (build order). When **more** lights overlap than **`maxPerTile`**:

- **`r_forwardPlusDistanceSort` 1** — partial selection by **distance²** to **`viewOrg`** (nearest lights win).
- Else **`r_forwardPlusLuminanceSort` 1** (default) — partial selection by **RGB sum** (brightest lights win).
- Else — first **`maxPerTile`** candidates in index order (legacy).

**Projection / coverage:**
- **Point:** screen-space sphere AABB vs tile (`sphere_tile_overlap`).
- **Linear/spot (`spotFrustumTileCull`):** coverage from origin + tip + segment mid, expanded by cone opening (`cos_outer` / `segLen` from packed records).

**Depth cull (`depthCull` / `r_forwardPlusDepthCull` 1 — `lightVolumeDepthCull`):** sample tile corners + center; reject when the light **volume** nearest Z is behind `sceneNearest` (reversed-Z). Not full Hi-Z yet — see roadmap Hi-Z phase.

---

## 5. Synchronization and pass placement

**Barriers in `vk_forward_plus_upload_refdef`:** **device-local** light SSBO: prior **SHADER_READ** (fragment/last frame) → **TRANSFER_WRITE**; **staging** **HOST_WRITE** → **TRANSFER_READ**; after copy, **TRANSFER_WRITE** → **SHADER_READ** for compute/fragment.

**Barriers in `vk_forward_plus_dispatch_tile_cull` / `_after_opaque`:**

1. **Before compute:** **light** buffer: **SHADER_READ** → **SHADER_READ** (hazard with prior frame; safe layout). **param** buffer: **HOST_WRITE** → **SHADER_READ**; tile buffer **dst** **SHADER_WRITE** (from prior fragment/compute read—first frame **`srcAccessMask = 0`**).
2. **After compute:** **SHADER_WRITE** → **SHADER_READ** on **tile** buffer for subsequent **VS/FS** (and compute if chained).
3. **Depth cull path:** **`record_depth_image_layout_transition`** to **`DEPTH_STENCIL_READ_ONLY_OPTIMAL`** before compute and back to **`DEPTH_STENCIL_ATTACHMENT_OPTIMAL`** after (main-pass depth).

**Compute inside render pass:** The dispatch is issued while **`vk.inRenderPass`** is true (main pass). This is **legal in Vulkan 1.x** when the pass does not use **subpasses** that forbid side effects; the engine uses **load/store** attachments and does not declare **subpass dependencies** that would make this invalid. **Risk:** some layers or future **render-pass graph** refactors could want compute **between** passes instead—worth revisiting if subpasses or **fragment density** are introduced.

**Host coherence:** **Staging** (light pack) and **param** buffers are **host-visible**; upload uses **transfer** for lights. **Param** still uses **`VK_PIPELINE_STAGE_HOST_BIT`** before compute.

---

## 6. Fragment path (`gen_frag.tmpl`)

**Gates:**

- **`USE_FORWARD_PLUS_FRAG`** / **`USE_FORWARD_PLUS_WORLD_POS`** — experimental shade and overlays require **world position** in the fragment stage.
- **`forward_plus_shade_strength`** — specialization constant; must stay in sync with **`vk_create_pipeline.c`** (Tier A check in **`renderer_regression_check.sh`**).

**Tile lookup:** Matches compute: **`tilePx`** from SSBO header, **`clip_from_world`** from **`fp_params`**, **`gl_FragCoord`**-style pixel mapping (same formula as compute). **`tbase = tileId * 8u`** — must stay equal to **`MAX_PER_TILE`** in **`forward_plus_tile_cull.comp`** (Tier A check).

**Energy / BRDF:** Additive pass uses **`CalcSpecular`** and a **renormalization** factor against **primary direct** (`fpRenorm`). This is explicitly **experimental**—not a second physically correct light transport path.

**`dlightBits` skip:** Prevents double-counting when the classic path already applied a dynamic light to this surface (first 32 bits only—documented limitation vs **`MAX_DLIGHTS`** if they ever diverge on other platforms).

---

## 7. Tier A regression coverage

`scripts/renderer_regression_check.sh` asserts:

- **`MAX_LIGHTS` == `VK_FP_MAX_GPU_LIGHTS`** (compute); **`MAX_DLIGHTS`** remains the surface **`dlightBits`** ceiling
- **`tr_world.c`** does **not** clamp **`tr.refdef.num_dlights`** (Forward+ pack uses full refdef count up to 64)
- **`MAX_PER_TILE` == `VK_FP_MAX_PER_TILE`**
- **`VK_FP_MIN_PER_TILE` ≤ `MAX_PER_TILE`**
- **`r_forwardPlusMaxPerTile`** CheckRange uses **`vk_forward_plus_get_*_per_tile_cap`**
- **`forward_plus_shade_strength`** `constant_id` matches **`ADD_FRAG_SPEC`**
- Compute uses **dynamic** tile pixels (no hard-coded **`16u`** tile corners)
- **PBR fragment tile stride** (`tileId * N`) matches **`MAX_PER_TILE`** from the compute shader
- **`VK_FP_TILE_DIM`** consistency (host grid)

---

## 8. Findings and recommendations

### Strengths

- **Single source of truth** for render resolution in packing/cull/shade: **`vk_get_render_target_width/height`** (+ cached main-color extent when FBO active).
- **Stale light** tail zeroing when counts drop.
- **Clip matrix** matches view/projection convention used elsewhere.
- **Dummy buffers** when Forward+ is off so set **18** stays valid for PBR pipelines.

### Risks / limitations (accepted for scaffolding)

| Item | Severity | Note |
|------|-----------|------|
| **Tile overload ordering** | Low–Medium (quality) | **`r_forwardPlusDistanceSort`** (nearest) or **`r_forwardPlusLuminanceSort`** (brightest, default when distance off) when a tile exceeds **`maxPerTile`**; else index order. |
| **Depth cull probes** | Low–Medium (quality) | **`r_forwardPlusDepthCull` 1** rejects lights behind the **nearest** of 5 tile probes (corners+center). Still open: full light-volume vs Hi-Z. |
| **Primary + Forward+ energy** | Medium (art) | Tunable via **`r_forwardPlusEnergyRenorm`** / **`r_forwardPlusSpecularStrength`** (defaults **0** / **0.65**; mode-2 owns dynamics so renorm stays off). |
| **Sphere screen approximation** | Low–Medium | Conservative enough for prototyping; not a tight spotlight frustum test. |
| **`dlightBits` 32-bit** | Low | Matches **`MAX_DLIGHTS`** today; document if caps change. |
| **Compute inside render pass** | Low (portability) | Valid now; revisit with subpass graphs or render graph. |

### Recent operational note

As of **July 18, 2026**, live renderer bisecting also showed that an otherwise working modern mode-2 stack could still transition into corrupted output and eventually **`VK_ERROR_DEVICE_LOST`** after late-frame post/bloom tuning changes. That result does **not** automatically prove Forward+ tile cull is the root cause, but it does reinforce the roadmap need for stronger pass-ownership diagnostics and safer late-post toggles around the shipping Vulkan path.

### Suggested next steps (roadmap)

1. **Depth-aware culling** — **partial:** **`r_forwardPlusDepthCull`** light-volume Z vs 5 probes; **`spotFrustumTileCull`** for linear lights. Still open: Hi-Z pyramid.
2. **Sort or priority** — **done for overload:** **`r_forwardPlusDistanceSort`** and **`r_forwardPlusLuminanceSort`** (see §4).
3. **Decouple** Forward+ light ceiling from **`MAX_DLIGHTS`** only if the **game protocol** and **`tess.dlightBits`** story are redesigned together.
4. **Tier B** map with mixed point + spot lights to validate heatmap vs ground truth.
5. **Energy without renorm** — single-path Forward+ ownership (mode 2) so **`r_forwardPlusEnergyRenorm`** can stay at 0.

---

## 9. Primary references

| File | Role |
|------|------|
| `src/renderers/vulkan/vk_forward_plus.c` | Packing, buffers, dispatch, tile resize |
| `src/renderers/vulkan/shaders/glsl/forward_plus_tile_cull.comp` | Tile list build |
| `src/renderers/vulkan/shaders/glsl/gen_frag.tmpl` | Debug + experimental shade |
| `src/renderers/vulkan/tr_shade.c` | `pbrForwardPlus` uniform |
| `src/renderers/vulkan/tr_backend.c` | Scheduling |
| `src/renderers/vulkan/vk_create_pipeline.c` | `forward_plus_shade_strength` spec |
| `scripts/renderer_regression_check.sh` | Tier A drift guards |
