# Compact G-buffer 2.0 Design

**Routing + bandwidth summary:** [GBUFFER_2.md](GBUFFER_2.md)

**Status:** Design + bandwidth reporting + dual-write prep (`r_gbufferCompact`, `gbuffer_octahedral.glsl`). Full layout migration is **not** shipping yet.  
**Related:** [BLACK_FRAME_REGRESSION.md](BLACK_FRAME_REGRESSION.md) · [RENDERER_IDTECH7_SPRINT.md](RENDERER_IDTECH7_SPRINT.md) · deferred fill in `vk_deferred_gbuffer.c`

**Prep (cheat):** `r_gbufferCompact 1` dual-writes octahedral into `material.ba` while normals stay scaffold XYZ; deferred lighting uses AO/clearcoat defaults when compact is on. `gbuffer_bandwidth` reports scaffold vs compact target B/px and Forward+ fallback %.

---

## Current scaffold (shipping)

| Target | Format | Bytes/px |
|--------|--------|----------|
| Albedo | `R16G16B16A16_SFLOAT` | 8 |
| Normal | `R16G16B16A16_SFLOAT` | 8 |
| Material | `R16G16B16A16_SFLOAT` | 8 |
| **Write total** | | **24** |

Deferred lighting typically reads all three + depth (~28 B/px read).

Report live: `gbuffer_bandwidth` / `renderer_resource_status` / `r_gbufferBandwidth 1`.

---

## Target layout (G-buffer 2.0)

| Target | Contents | Suggested format | Bytes/px |
|--------|----------|------------------|----------|
| GBuffer0 | base color RGB + material flags | `R8G8B8A8_UNORM` or `A2B10G10R10` | 4 |
| GBuffer1 | octahedral normal (2) + perceptual roughness + metallic | `R8G8B8A8_UNORM` | 4 |
| GBuffer2 | emissive (RGBE or RGB) + AO + extension index | `R8G8B8A8_UNORM` or `R16G16B16A16_SFLOAT` for HDR emissive | 4–8 |
| **Write target** | | | **12–16** |

Separate (not every opaque pixel):

- velocity
- temporal classification
- object / material identity
- authoritative reversed-Z depth

Rare features (anisotropy, transmission, complex coats, refraction, SSS, water, skin) → **Forward+** via `R_SelectSurfaceRenderPath` / `r_materialPathReason`.

---

## Standard deferred materials (keep)

- metallic/roughness, dielectric specular, normal maps  
- clear coat, sheen, material AO, emissive  
- lightmaps, deluxe maps, probe lighting  

## Forward+ fallback (do not enlarge G-buffer)

- anisotropy, transmission, layered coatings, refraction  
- subsurface variants, water, specialized skin  

---

## Bandwidth acceptance

| Metric | Scaffold now | G-buffer 2.0 goal |
|--------|--------------|-------------------|
| Write B/px | 24 | ≤ 16 |
| Deferred read B/px | ~28 | ≤ 20 |
| Forward+ fallback % | measure via path status | track; rare features only |

---

## Cvars / commands

| Name | Role |
|------|------|
| `r_gbufferBandwidth` | Auto-print once per session when set |
| `r_gbufferDebug` | Naming alias (prefer `r_deferredGBufferDebug` for composite) |
| `r_materialPathReason` | Log Deferred vs Forward+ routing (developer) |
| `gbuffer_bandwidth` | Print bytes/px and MiB/frame |

---

## Migration plan (do not land in one PR)

1. Octahedral encode/decode helpers + parity tests vs world normals.  
2. Dual-write period: fill compact + scaffold; deferred lighting reads compact.  
3. Drop scaffold attachments when parity green.  
4. Extension index → material buffer / Forward+ for rare lobes.
