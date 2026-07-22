# Renderer Path Ownership

**Status:** Milestone 1 (Clustered Hybrid)  
**Audience:** Anyone choosing which surface path owns shading / lighting for a draw

## Shipping defaults

| Mode | Role | Default? |
|------|------|----------|
| **0** | Classic forward (projector) | no |
| **1** | Deferred split (opaque deferred + Forward+ transparent) | opt-in (`deferred_vulkan.cfg`) |
| **2** | Forward+ primary | **yes** — `modern_vulkan.cfg` → `modern_vulkan_stable.cfg` |
| **3** | Unified Clustered Hybrid | opt-in (`modern_clustered.cfg`) |
| **4** | Tier B Selective Hybrid (RTX) | opt-in — **do not reclaim for vis-buffer** |
| **5** | Tier C path-traced reference | opt-in |

Visibility-buffer late shade remains an **opt-in sidecar** on modes 1–3 (`r_visibilityBuffer` / `r_visibilityLateShade`), not a new `r_renderMode`.

Canonical selector: `R_SelectSurfaceRenderPath()` in [`renderers/vulkan/vk_render_path.c`](../renderers/vulkan/vk_render_path.c). Debug: `r_renderPathDebug`, `render_path_status`.

## Mode 3 surface-class ownership

| Class | Owner |
|-------|--------|
| OPAQUE_STANDARD | Deferred compute (opaque handoff skips Forward+ tile add) |
| OPAQUE_ALPHA_TESTED | Deferred if depth-written in G-buffer fill; else Forward+ opaque fallback |
| OPAQUE_COMPLEX / refractive / clearcoat-heavy | Forward+ opaque fallback |
| TRANSLUCENT / PARTICLE | Forward+ transparent (or OIT when `r_oit` on) |
| WEAPON (first-person / depth-hack) | Forward+ after world (Architecture B) |
| SKY / WATER / VOLUMETRIC | Existing specialized paths |
| UI | Overlay after tonemap |

Modes **1** and **3** share the opaque→deferred→transparent split when deferred lighting is path-ready. Mode **2** keeps Forward+ on opaque (no deferred handoff). If deferred capture/lighting/composite is not ready, handoff **fails open** to Forward+ so the frame cannot go black.

## Shared cluster grid

Deferred lighting, Forward+ shade, and OIT (`r_oitForwardPlus`) consume the **same** Forward+ light + tile/Z-cluster SSBOs (set 18 / `forward_plus_cluster.glsl`).

| Alias cvar | Backs onto |
|------------|------------|
| `r_clusterZSlices` | `r_forwardPlusZSlices` |
| `r_clusterTileSize` | fixed **16** (validated at init) |
| `r_clusterDebug` | `r_forwardPlusDebug` |

Clustered hybrid default: `r_forwardPlusZSlices 8` via `modern_clustered.cfg`.

Parity gate: `vk_cluster_assert_shared_consumers()` — deferred lighting and Forward+ fragment must bind the same tile buffer + generation.

## Debug

| Cvar | Meaning |
|------|---------|
| `r_renderPathDebug 0` | off |
| `1` | Tint shaded HDR by selected `renderPath_t` |
| `2` | Also accumulate per-frame path counts (`render_path_status`) |
| `r_hybridCompare 1` | Split-screen: left deferred opaque, right Forward+ opaque |

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
