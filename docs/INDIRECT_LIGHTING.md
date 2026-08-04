# Indirect Lighting Contract

**Status:** Foundation Consolidation — baked + dynamic indirect sources.  
**Source:** `vk_indirect_light.c` · **Related:** lightmaps, deluxe maps, GTAO, irradiance probes

The screen-space component is informed by Soler, Hoel, and Rochet,
*A Deferred Shading Pipeline for Real-Time Indirect Illumination* (SIGGRAPH
2010, HAL `inria-00480869v3`). The current implementation uses the paper's
important ownership ideas—G-buffer inputs, constant-cost screen-space samples,
confidence routing, temporal reuse, and additive deferred composition—without
claiming the paper's full multi-resolution gather yet.

---

## Ownership

| Source | Owner | Role |
|--------|-------|------|
| BSP lightmaps | `tr_bsp.c` / merged atlases | Static diffuse base |
| Deluxe maps | Lightmap MRT | Directional baked detail |
| Sky / ambient probe | Atmosphere / cubemap | Outdoor ambient |
| Irradiance probes | `vk_indirect_light.c` | Local `vkIrradianceProbe_t` SSBO |
| GTAO / AO | Ambient visibility passes | Screen-space occluder |
| Neural (opt-in) | NIV / NDGI / surfel GI | Experimental additive |

Indirect **composite** happens in deferred lighting + Forward+ eval — not a single buffer owner.

---

## Data flow

```text
Lightmap sample (uv) + deluxe direction
  + probe SH / irradiance (vk_indirect_light_probe)
  + GTAO factor (if r_gtao / AV on)
  + sky ambient (outdoor)
  → indirect diffuse added to direct BRDF result
r_indirectDebug 1–6 → probe spheres, irradiance, SH, cache, leak, atlas occupancy

`r_ssgi 1` is the near-field screen-space consumer. It samples current-frame
depth, normals, SceneHDR, and albedo, then confidence-routes the result against
probe/cache indirect energy. This keeps static lightmaps and dynamic indirect
lighting from being added twice.
```

Probes set via `vk_indirect_light_set_probe()` — generation bumps on explicit update.

---

## Buffer formats

**vkIrradianceProbe_t** (max 256):

| Field | Type |
|-------|------|
| position | `vec3` |
| irradiance | `vec3` |
| radius | `float` |
| generation, flags | `uint32` |

Lightmaps: RGB8/RGB16 atlas (engine standard). GTAO: R8 or R16F single channel. Debug modes false-color SceneHDR.

---

## Lifecycle

1. Map load — lightmap atlas upload; probes cleared or loaded from manifest.
2. `vk_indirect_light_register()` — `r_indirectDebug`, `indirect_light_status`.
3. Per frame: `vk_indirect_light_begin_frame()` — probes persist.
4. Dynamic GI (NDGI/NIV) optional overlay — separate generations.

---

## Fallback behavior

- No deluxe map → lightmap RGB only.
- No probes → sky + lightmap ambient.
- GTAO off → material AO from G-buffer only.
- Probe leak / stale generation — debug modes 5–6 highlight; no crash.

---

## Debug commands

| Cvar / command | Role |
|----------------|------|
| `r_indirectDebug` | 1 probe spheres, 2 irradiance, 3 SH bands, 4 cache, 5 leak test, 6 atlas occupancy |
| `indirect_light_status` | Probe count, generation, first 8 probe dump |

---

## Performance cost

| Source | Cost |
|--------|------|
| Lightmap sample | 1–2 texture fetches (cached) |
| GTAO | 1–3 ms @ 1080p when enabled |
| Probes | O(probes × objects) CPU set; GPU sample cheap |
| Neural GI | Profile-specific (experimental) |

---

## Known limitations

- `vk_indirect_light.c` scaffold — probes CPU-side; GPU SSBO upload path partial.
- Deluxe / lightmap merge policy map-specific.
- Leak test debug (mode 5) manual — not automated CI yet.
- The current SSGI gather is single-level. Hierarchical mip-level gathering and
  bilateral upsampling remain a follow-up; `textureLod` use alone is not proof
  that the full paper algorithm is active.

---

## Next milestone hooks

- GPU probe atlas + `r_indirectDebug 6` occupancy from real backing store.
- Reference lab GI scene automated capture.
- Hierarchical SSGI gather: fixed-cost multi-resolution samples, bilateral
  upsample, and temporal confidence validation.
- Parity: lightmap + probe vs path-traced reference (offline).

Regression: `tests/scripts/test_indirect_light_parity.sh`
