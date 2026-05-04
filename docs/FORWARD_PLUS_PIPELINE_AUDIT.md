# Forward+ render pipeline — audit (Vulkan, 2026)

This document is a **technical audit** of the current **Forward+ scaffolding** in this fork: what runs, what data flows where, synchronization, known limitations, and **risk items** for future work. It complements the narrative in [RENDERER_2026_ARCHITECTURE_PASS.md](RENDERER_2026_ARCHITECTURE_PASS.md).

**Scope:** `r_forwardPlus` (default **1** on Vulkan, **latched**), PBR-only descriptor integration, **dynamic lights** from `backEnd.refdef` (`dlight_t`), **no** replacement of the primary forward lighting path.

---

## 1. Feature summary

| Layer | Responsibility |
|--------|----------------|
| **C / `vk_forward_plus.c`** | CPU packs **staging** (host) light records; **device-local** light SSBO via `vkCmdCopyBuffer` each frame; **tile SSBO** allocation, **param SSBO** (`clipFromWorld` + aux uvec4), compute **pipeline + dispatch**, graphics **descriptor set** (set **18**), tile grid from **`VK_FP_TILE_DIM`** (16 px) and **`vk_get_render_target_width/height`**. |
| **Compute / `forward_plus_tile_cull.comp`** | Per-tile **light index lists** ( **`MAX_PER_TILE` = 8** ), sphere-in-screen projection cull, **`MAX_LIGHTS` = 32** aligned with **`MAX_DLIGHTS`**. |
| **Fragment / `gen_frag.tmpl`** (PBR) | Optional **debug heatmap** (`r_forwardPlusDebug`), optional **additive experimental shade** (`r_forwardPlusShade` → specialization **`forward_plus_shade_strength`**). Uses **`fp_params.fp_clip_from_world`** and SSBO light + tile data. |
| **Uniform bridge / `tr_shade.c`** | When Forward+ is on, **`pbrForwardPlus.y`** carries **`floatBitsToUint(tess.dlightBits)`** so the fragment path can **skip** culled lights that the surface already received via the classic packed path (first **32** indices). |

**Cvars** (see `tr_init.c`): `r_forwardPlus`, `r_forwardPlusMaxPerTile` (latched **4–8**), `r_forwardPlusDebug`, `r_forwardPlusShade` (pipeline invalidation on change in `vk_frame_submit.c`), `r_forwardPlusLuminanceSort` (**0/1**, default **1** — tile overload picks brightest lights by RGB sum).

---

## 2. Frame / command ordering

Within **`RB_DrawSurfs`** (`tr_backend.c`), order is:

1. **`vk_prepare_frame_temporal_state()`**
2. **`vk_forward_plus_ensure_render_resolution()`** — may resize **tile SSBO** if render target dimensions changed (matches FBO / `r_renderScale` via **`vk_get_render_target_*`**).
3. **`vk_forward_plus_update_for_refdef()`** — CPU writes the **staging** buffer with light header + records; clears **tail** when count drops.
4. **`RB_RenderSunShadowMap`**
5. **`RB_BeginDrawingView()`** — begins the **main** render pass.
6. **`vk_forward_plus_upload_refdef()`** — `vkCmdCopyBuffer` staging → **device-local** light SSBO (transfer + shader barriers).
7. **`vk_forward_plus_dispatch_tile_cull()`** — **compute** inside the active render pass (see §5).

Then the world/entity draws run; PBR draws bind **descriptor set 18** when Forward+ resources are live (`vk_draw_state.c`).

---

## 3. Data layout (SSBOs)

### 3.1 Light buffer (`binding = 0`)

Packed as **`float`** array in **`vk_forward_plus_update_for_refdef`**:

| Offset (vec4 index) | Content |
|---------------------|---------|
| `data[0]` | **x** = packed light count **n**, **y** = refdef time (ms), **z** = **`max_per_tile`** (effective **4–8**), **w** = debug scale |
| `data[1]` | **x,y** = **`tiles_x`, `tiles_y`**, **z,w** = viewport **width/height** (render target pixels) |
| `data[2 + i*4 …]` | Four **`vec4`** per light **i** (origin+radius, color+linear flag, axis/cone pack, etc.) — mirrors **`dlight_t`** fields |

**Caps:** at most **`MAX_DLIGHTS` (32)** lights for index compatibility with **`tess.dlightBits`**. Packing may be further limited by **buffer capacity**; overflow is clamped with a **developer** log (rate-limited by last source count).

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

**Ordering / overload:** when a tile has **fewer** overlapping lights than **`maxPerTile`**, output order matches **increasing light index** (build order). When **more** lights overlap than **`maxPerTile`**, and **`r_forwardPlusLuminanceSort`** is **1** (default), the shader runs a **partial selection** on the candidate list to keep the top **`maxPerTile`** by **RGB sum** (from the packed color **vec4**). If **`r_forwardPlusLuminanceSort`** is **0**, the first **`maxPerTile`** candidates in index order are kept (legacy overload behavior).

---

## 5. Synchronization and pass placement

**Barriers in `vk_forward_plus_upload_refdef`:** **device-local** light SSBO: prior **SHADER_READ** (fragment/last frame) → **TRANSFER_WRITE**; **staging** **HOST_WRITE** → **TRANSFER_READ**; after copy, **TRANSFER_WRITE** → **SHADER_READ** for compute/fragment.

**Barriers in `vk_forward_plus_dispatch_tile_cull`:**

1. **Before compute:** **light** buffer: **SHADER_READ** → **SHADER_READ** (hazard with prior frame; safe layout). **param** buffer: **HOST_WRITE** → **SHADER_READ**; tile buffer **dst** **SHADER_WRITE** (from prior fragment/compute read—first frame **`srcAccessMask = 0`**).
2. **After compute:** **SHADER_WRITE** → **SHADER_READ** on **tile** buffer for subsequent **VS/FS** (and compute if chained).

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

- **`MAX_LIGHTS` == `MAX_DLIGHTS`**
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
| **No light sort** in tile lists | Medium (quality) | First-N lights win per tile **unless** `r_forwardPlusLuminanceSort` is **1** (default): then overloaded tiles keep top **`maxPerTile`** by RGB sum. |
| **Sphere screen approximation** | Low–Medium | Conservative enough for prototyping; not a tight spotlight frustum test. |
| **`dlightBits` 32-bit** | Low | Matches **`MAX_DLIGHTS`** today; document if caps change. |
| **Compute inside render pass** | Low (portability) | Valid now; revisit with subpass graphs or render graph. |
| **Primary + Forward+ energy** | Medium (art) | Renormalization is heuristic; tune per title if shade is enabled. |

### Suggested next steps (roadmap)

1. **Depth-aware culling** (optional Hi-Z or linear depth rejection) before accepting a light for a tile.
2. **Sort or priority** (distance / luminance) when filling **`maxPerTile`** slots — **partially done:** **`r_forwardPlusLuminanceSort`** (default **1**) uses **RGB sum** when overloaded; distance-based priority is still open.
3. **Decouple** Forward+ light ceiling from **`MAX_DLIGHTS`** only if the **game protocol** and **`tess.dlightBits`** story are redesigned together.
4. **Tier B** map with mixed point + spot lights to validate heatmap vs ground truth.

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
