# Renderer Path Ownership

**Status:** Milestone 2 (Clustered Hybrid) — see also [CLUSTERED_LIGHTING.md](CLUSTERED_LIGHTING.md)  
**Audience:** Anyone choosing which surface path owns shading / lighting for a draw

## Shipping defaults

| Mode | Role | Default? |
|------|------|----------|
| **0** | Classic forward (projector) | no |
| **1** | Deferred split (opaque deferred + Forward+ transparent) | opt-in (`deferred_vulkan.cfg`) |
| **2** | Forward+ legacy recovery | no — `gfx_safe.cfg` / low-latency fallback |
| **3** | Unified Clustered Hybrid | **yes** — `modern_vulkan.cfg` → `modern_vulkan_stable.cfg` |
| **4** | Tier B Selective Hybrid (RTX) | opt-in — **do not reclaim for vis-buffer** |
| **5** | Tier C path-traced reference | opt-in |

Visibility-buffer late shade remains an **opt-in sidecar** on modes 1–3 (`r_visibilityBuffer` / `r_visibilityLateShade`), not a new `r_renderMode`.

Canonical selector: `R_SelectSurfaceRenderPath()` in [`renderers/vulkan/vk_render_path.c`](../renderers/vulkan/vk_render_path.c). Debug: `r_renderPathDebug`, `render_path_status`.

Mode metadata lives in `renderModeProfile_t` via `R_RenderMode_ProfileForValue()` /
`R_RenderMode_CurrentProfile()` in [`renderers/vulkan/tr_render_mode_vk.c`](../renderers/vulkan/tr_render_mode_vk.c).
Renderer diagnostics use this table to print the requested mode name, tier, and
feature contract (`Forward+`, G-buffer, deferred lighting, opaque/transparent
split, path tracing, production default) without duplicating mode rules.

## Mode 3 surface-class ownership

**Honest label:** when deferred lighting is active, architecture is selected by **`r_deferredArchitecture`** — see [DEFERRED_HONESTY.md](DEFERRED_HONESTY.md). Default **`HYBRID_ADDITIVE_DEFERRED`**: fragment paths write **SceneBaseLit**; deferred compute adds dynamics. **`MIXED_MATERIAL_DEFERRED`** (arch 1): eligible surfaces export unlit G-buffer + deferred lightmap ownership.

| Class | Owner |
|-------|--------|
| OPAQUE_STANDARD (PBR native or translated classic) | Deferred handoff + hybrid additive composite |
| OPAQUE classic multi-stage / env / incomplete export | **Forward+** (`R_GetDeferredEligibility`) |
| OPAQUE_ALPHA_TESTED (translated) | Deferred approx if eligible; else Forward+ |
| OPAQUE_COMPLEX / refractive | Forward+ / unsupported |
| TRANSLUCENT / PARTICLE | Forward+ transparent (or OIT when `r_oit` on) |
| WEAPON (first-person / depth-hack) | Forward+ after world (Architecture B) |
| SKY / WATER / VOLUMETRIC | Existing specialized paths |
| UI | Overlay after tonemap |

Modes **1** and **3** share the opaque→deferred→transparent split when deferred lighting is path-ready. Mode **2** keeps Forward+ on opaque (no deferred handoff). If deferred capture/lighting/composite is not ready, handoff **fails open** to Forward+ so the frame cannot go black. Console: `deferred_status`.

## Shared cluster grid

Deferred lighting, Forward+ shade, and OIT (`r_oitForwardPlus`) consume the **same** Forward+ light + compact header/index (or legacy tile) SSBOs (`cluster_contract.glsl` / `cluster_light_list.glsl`).

| Alias / cvar | Backs onto |
|--------------|------------|
| `r_clusterZSlices` | `r_forwardPlusZSlices` |
| `r_clusterTileSize` | fixed **16** (validated at init) |
| `r_clusterDebug` | `r_forwardPlusDebug` (mode **6** = Z-slice + crosshair) |
| `r_clusterZFar` | clamp for log-Z far (default 4096; `min` with camera zFar) |
| `r_clusterCompactLists` | compact header+index lists (−1 auto / 0 legacy / 1 force) |

Clustered hybrid default: `r_forwardPlusZSlices 8` via `modern_clustered.cfg`.

Parity gate: `vk_cluster_assert_shared_consumers()` — deferred lighting and Forward+ fragment must bind the same tile/header buffer + generation.

Details: [CLUSTERED_LIGHTING.md](CLUSTERED_LIGHTING.md).

## Debug

| Cvar | Meaning |
|------|---------|
| `r_renderPathDebug 0` | off |
| `1` | Tint shaded HDR by selected `renderPath_t` |
| `2` | Also accumulate per-frame path counts (`render_path_status`) |
| `r_hybridCompare 1–8` | Deferred vs Forward+ compare (split / abs / luma / …) — see CLUSTERED_LIGHTING.md |

## GPU scene / material schema (contract)

Milestone 1 documents the contract only; full material SSBO is Milestone 2.

### Instance (`vkGpuSceneInstance_t`)

Required / reserved fields already present:

- `meshId`, `materialId`, `objectId`
- `transform[12]`, `prevTransform[12]` (motion / temporal)
- bounds + sphere, LOD, `flags`, `streamState`, `visibleAge`, `lastReject`, `generation`

Reserved for M2 (do not reinterpret without bumping generation):

- High bits of `flags` for temporal class / material path reason
- Future: explicit `temporalClass` / `materialPathReason` uint32s when SSBO layout expands

### Mesh (`vkGpuSceneMesh_t`)

- `materialId`, meshlet/index ranges, `flags` (alpha-test / skinned / hard-edge), bounds

### Forward+ light record (CPU ↔ shader)

- Header: 2× `vec4` (`VK_FP_HEADER_BYTES`)
- Per light: 4× `vec4` / 16 floats (`VK_FP_RECORD_STRIDE`)
- Max lights: `VK_FP_MAX_GPU_LIGHTS` (64)
- Tile/cluster list: `VK_FP_MAX_PER_TILE` (8) `uint32` indices per cluster; tile size **16×16**

Asserted at Forward+ init (`vk_forward_plus_init`).

## Related docs

- [UNIFIED_CLUSTERED_RENDERER.md](UNIFIED_CLUSTERED_RENDERER.md)
- [RENDERERS.md](RENDERERS.md)
- [RENDERER_2027.md](RENDERER_2027.md)
- [RENDERER_SPINE_1.0.md](RENDERER_SPINE_1.0.md)
