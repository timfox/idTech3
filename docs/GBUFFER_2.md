# Compact G-Buffer 2 (Routing + Bandwidth)

**Status:** Foundation Consolidation — compact layout design + material path routing.  
**Design detail:** [GBUFFER_2_0.md](GBUFFER_2_0.md) · **Routing:** `vk_render_path.c` · **Fill:** `vk_deferred_gbuffer.c`

Summarizes the **target** compact G-buffer, bandwidth budgets, and **`R_SelectSurfaceRenderPath`** (material render path) for deferred vs Forward+ handoff.

---

## Ownership

| Attachment | Writer | Reader |
|------------|--------|--------|
| GBuffer albedo | Deferred fill / dual-write compact | Deferred lighting, debug |
| GBuffer normal | Deferred fill (scaffold XYZ or octahedral) | Deferred lighting |
| GBuffer material | Deferred fill | Deferred lighting, extension index |
| GBuffer lighting | Deferred lighting compute | Scene composite |
| Depth | Opaque prepass | All deferred + Forward+ tile cull |

Rare materials (anisotropy, transmission, water, refraction) owned by **Forward+** — not expanded into G-buffer.

---

## Data flow

```text
R_SelectSurfaceRenderPath(shader, surface, flags, viewClass)
  → RENDER_PATH_DEFERRED_OPAQUE | FORWARD_PLUS_* | OIT | WEAPON | SKY | UI
Deferred opaque → G-buffer fill → deferred lighting → SceneHDR
Forward+ opaque → clustered shade (reads depth, not full G-buffer)
Complex / transparent → Forward+ or OIT (see r_oit, mode 3)
```

`R_RenderPath_Note()` feeds `render_path_status` counts. `r_materialPathReason` logs routing reason string (developer).

---

## Buffer formats

**Target compact (G-buffer 2.0)** — see [GBUFFER_2_0.md](GBUFFER_2_0.md):

| Target | Format | B/px |
|--------|--------|------|
| G0 base + flags | `R8G8B8A8_UNORM` | 4 |
| G1 normal + rough + metal | octahedral in `R8G8B8A8_UNORM` | 4 |
| G2 emissive + AO + ext | `R8G8B8A8_UNORM` or FP16 emissive | 4–8 |
| **Write target** | | **12–16** |

**Shipping scaffold:** 24 B/px write (FP16×3). Dual-write: `r_gbufferCompact 1` packs octahedral into material.ba (direct MRT + depth fill) and AO into normal.a; deferred lighting decodes oct. Clearcoat still defaults to 0 until G2 cutover.

Separate: velocity, temporal class, object id, reversed-Z depth.

---

## Lifecycle

1. Deferred path ready when `r_renderMode` 1/3/4 + FBO + split active.
2. G-buffer allocated on demand; generation in `vk.deferredGbufferGeneration`.
3. Fill pass runs before deferred lighting; generation bumped on resize / `vid_restart`.
4. Compact dual-write period: both scaffold + compact fields populated.
5. Cutover (future): drop scaffold attachments when parity green.

---

## Fallback behavior

- Classic lighting (`R_ClassicLightingActive`) → `RENDER_PATH_LEGACY_FORWARD`.
- Complex opaque shader → Forward+ even in deferred mode.
- Deferred not ready → Forward+ opaque fallback.
- `r_gbufferCompact 1` with scaffold readers → AO/clearcoat defaults in lighting when compact on.

Forward+ fallback percentage reported by `gbuffer_bandwidth` / `render_path_status verbose`.

---

## Debug commands

| Command / cvar | Role |
|----------------|------|
| `gbuffer_bandwidth` | Scaffold vs compact B/px, MiB/frame, Forward+ fallback % |
| `render_path_status verbose` | Per-path surface counts |
| `r_renderPathDebug` | False-color path overlay |
| `r_materialPathReason` | Log `R_SelectSurfaceRenderPath` reason |
| `r_gbufferCompact` | Dual-write compact prep |
| `r_deferredGBufferDebug` | Composite G-buffer debug view |

---

## Performance cost

| Layout | Write B/px | Deferred read B/px |
|--------|------------|-------------------|
| Scaffold (now) | 24 | ~28 |
| Compact goal | ≤16 | ≤20 |

Bandwidth savings ~30–40% at 4K when compact cutover lands. Routing overhead (path select) negligible — per-surface CPU branch.

---

## Known limitations

- Full layout migration **not** shipping; scaffold remains authoritative for lighting.
- `R_SelectMaterialRenderPath` naming in sprint docs aliases `R_SelectSurfaceRenderPath`.
- Octahedral parity vs world normals under test (`gbuffer_octahedral.glsl`).
- Extension index for rare lobes not wired to material buffer yet.

---

## Next milestone hooks

- Land `gbuffer_octahedral.glsl` + `test_gbuffer_layout.sh` parity.
- Deferred lighting read compact attachments exclusively.
- Material extension index → Forward+ handoff table.
- Auto-fail CI when Forward+ fallback % exceeds budget on stock maps.

Regression: `tests/scripts/test_gbuffer_layout.sh` · `tests/scripts/test_material_routing.sh`
