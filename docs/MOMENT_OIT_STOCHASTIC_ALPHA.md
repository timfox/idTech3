# Order-Independent Transparency (WBOIT Production)

**WBOIT (`r_oit 1`) is the production transparency path** for glass, smoke, particles, and overlapping translucent layers. **MBOIT (`r_oit 2`) remains experimental** — see [Appendix: MBOIT](#appendix-mboit-moment-transparency-r_oit-2) and [OIT_FUTURE_TRACKS.md](OIT_FUTURE_TRACKS.md).

| Mode | Class | Technique |
|------|-------|-----------|
| `r_oit 0` | Off | Sorted alpha blend |
| `r_oit 1` | **Production** | **WBOIT** (weighted blended OIT) |
| `r_oit 2` | Experimental | MBOIT / Moment Transparency |

Requires `r_fbo 1` and `vid_restart`.

## Quick start (production)

```
seta r_fbo 1
seta r_oit 1
seta r_oitForwardPlus 1
vid_restart
```

Unified Clustered (mode 3) shipping overlay:

```
exec vulkan_overlay_oit_clustered.cfg
vid_restart
```

Spine 1.1 certification pins WBOIT:

```
exec vulkan_overlay_spine_1_1_cert.cfg
vid_restart
```

Demos: `exec demo_wboit_stress_mode3.cfg`, `exec demo_wboit_parity.cfg`, `exec demo_oit_clustered.cfg`.

## WBOIT algorithm

1. Opaque geometry (depth write)
2. **Accum pass** — McGuire/Bavoil weighted color + revealage (`R16G16B16A16` + `R16` reveal)
3. **Resolve** — composite onto opaque HDR background

Optional Forward+ lit accum (`r_oitForwardPlus 1`, default): accumulation samples the same cluster tile lists as deferred opaque, using shared Burley+GGX eval (`forward_plus_light_eval.glsl`).

Resolve equation (linear HDR):

```
C_avg = accum.rgb / max(accum.a, eps)
C_out = C_avg * (1 - revealage) + C_bg * revealage
```

Clears: accum `vec4(0)`, revealage `1`. Depth test uses reversed-Z `GREATER_OR_EQUAL` (no depth write).

### Pass order (world)

Opaque → deferred → **OIT accum → OIT resolve** → refractive (when `r_refractiveExcludeOit 1`) → weapon (`RDF_NOWORLDMODEL`) → post → UI.

**First-person weapons are excluded from world OIT targets.** Resolve runs before the weapon pass; weapon draws never write accum/reveal.

### With Unified Clustered (`r_renderMode 3`)

When `r_oit 1` is on, the backend runs **`vk_oit_pass` instead of** the Forward+ transparent shade pass. Cluster generation is shared with deferred via `vk_cluster_assert_shared_consumers( "oit_accum" )`. See [UNIFIED_CLUSTERED_RENDERER.md](UNIFIED_CLUSTERED_RENDERER.md).

`r_oitClassify 1` splits alpha-blend vs additive particles (additive uses a dedicated accum pipeline with reveal write-mask off). Default `0` keeps a single bucket. Hair cards stay on `r_stochasticAlpha`, not OIT.

## Promotion table

| Capability | Class |
|------------|--------|
| WBOIT (`r_oit 1`) unlit | **Production** |
| WBOIT Forward+ lit accum | **Production** |
| WBOIT classify buckets | **Production** (opt-in via `r_oitClassify 1`) |
| Mode 3 + WBOIT overlay | **Production** (`vulkan_overlay_oit_clustered.cfg`) |
| Spine 1.1 cert stack | **Production** (`vulkan_overlay_spine_1_1_cert.cfg`, `r_oit 1`) |
| Weapon / world separation | **Production** (resolve-before-weapon) |
| MBOIT (`r_oit 2`) | **Experimental** — not Spine 1.1 certified |
| Boot `modern_vulkan.cfg` stable spine | Unchanged Forward+ mode 2 fallback |

Future / research tracks: [OIT_FUTURE_TRACKS.md](OIT_FUTURE_TRACKS.md).

## Certification config

Spine 1.1 pins mode 3 + WBOIT + Temporal Reconstruction + weapon-after-TAA:

| Pin | Value |
|-----|-------|
| `r_renderMode` | 3 |
| `r_oit` | **1** (WBOIT) |
| `r_oitForwardPlus` | 1 |
| Entry | `exec vulkan_overlay_spine_1_1_cert.cfg` |

Full cert invariants: [RENDERER_SPINE_1.1.md](RENDERER_SPINE_1.1.md).

## Console diagnostics

| Command / cvar | Purpose |
|----------------|---------|
| `oit_status` | Frame state, generations, extents, cluster gen, unhealthy flags |
| `oit_perf` | CPU markers for clear / accum / resolve |
| `oit_capture stages` | Stage capture helper |
| `r_oitDebug` 0–16 | Resolve-stage views (cheat); 14/15 = band/tile isolation |
| `r_oitDirectTest` 0–2 | Clear+resolve without transparent draws (cheat) |
| `r_oitExtentDebug` | Extent/viewport/generation overlay (cheat) |
| `r_oitLightingDebug` 1–8 | Lit accum term views / BRDF-diff vs opaque (cheat) |
| `r_oitParityCompare` | Near-opaque lit-term split compare (cheat) |
| `r_oitClusterDebug` 1–5 | Cluster handoff / generation mismatch views (cheat) |
| `r_oitForce*` | Fault injection for lifecycle tests (cheat) |

Static gates: `./scripts/oit_corruption_check.sh`, `tests/scripts/test_wboit_*.sh`.

## WBOIT soak matrix (B0–B7)

Device soak — stop at first band/tile failure. No invented timings; report measured duration only if run.

| Step | Setting | Expected if fix holds |
|------|---------|------------------------|
| B0 | `r_oit 0` | Clean opaque/weapon/UI |
| B1 | WBOIT raw (`r_oitForwardPlus 0`, `r_oitClassify 0`) | No rectangular bands |
| B2 | `r_oitClassify 1` | No mid-bucket bands |
| B3 | `r_oitForwardPlus 1` | No magenta tile slabs (OOB) |
| B4 | `r_oitDebug` 1–13 | Stage views coherent |
| B5 | `r_oitDebug` 14 | Magenta×coverage only (ignore accum RGB) |
| B5b | `r_oitDebug` 15 | Smooth FragCoord UV (no bands) |
| B6 | `r_oitDirectTest 1` | Pure opaque after clear |
| B6b | `r_oitDirectTest 2` | Smooth half UV-gradient composite |
| B7 | `cg_drawGun 0/1` | Gun clean when world resolve clean |

**Odd extents (device):** exercise native resolution plus non-even sizes — **1919×1079**, **1921×1081**, **1279×719**, **1281×721** (`r_renderWidth` / `r_renderHeight` or window resize). Also useful: 1365×767, 1601×901. Lifecycle: `vid_restart`, map change. Tile indices are clamped in `forward_plus_light_eval.glsl`; use `r_oitExtentDebug 1` after `sv_cheats 1`.

**Isolation entry points:**

- `exec demo_oit_isolation.cfg`
- `exec repro_oit_corruption.cfg` (also `./scripts/repro_oit_corruption.sh`)
- `exec demo_wboit_stress_mode3.cfg`

Shipped into `release/base/`, `release/openarena/`, `release/havenrp/` via `compile_engine.sh`.

## Temporal reconstruction

When `r_taa` / `r_aaMode` 4–5 is active, OIT revealage stamps a dedicated R8 **reactive mask** so Temporal Reconstruction prefers the current frame on glass/smoke (`r_temporalReactiveMask 1`). Raw OIT accum/reveal are never temporally blended — resolve into HDR first. First-person weapons are deferred past world TAA (`r_temporalWeaponAfterTaa 1`). See [HDR_GAPS.md](HDR_GAPS.md) §6.8.

## Corruption fixes (WBOIT lifecycle)

Key hardening (resolve layout, frame state, weapon exclusion, additive reveal mask, cluster OOB):

- Resolve RP: `initialLayout=UNDEFINED`, `loadOp=DONT_CARE`, `finalLayout=SHADER_READ_ONLY`
- `oitFrameState`: UNTOUCHED → CLEARED → ACCUMULATED → RESOLVED; refuse resolve from UNTOUCHED
- Single `oitAttachmentGeneration` bump after FB creation; descriptors must match
- Extent triad check before resolve; weapon flush asserts OIT flags cleared
- Additive ONE/ONE particles: dedicated accum pipeline (reveal write-mask off)
- WBOIT weights clamped `[5e-2, 3e3]`; soft-alpha discard floor `1e-3`

**First corrupt producer (isolation):** `oit_resolve` writing `color_image` under `DONT_CARE` with wrong prior layout — weapon contamination = same resolved HDR under the gun.

**Reconnect / Unpure crash (`vid_restart`):** repro launches with `+set sv_pure 0` (`sv_pure` is latched). See `repro_oit_corruption.cfg` comments.

---

## Appendix: MBOIT / Moment Transparency (`r_oit 2`)

**Experimental only.** Startup warns: `MBOIT is experimental and not Spine 1.1 certified.` Use `vulkan_overlay_mboit.cfg` or `modern_vulkan_experimental.cfg`.

### Algorithm (mode 2)

1. Opaque geometry (depth write)
2. **Moments pass** — optical depth `d = -log(1-α)` and power moments `d·(z,z²,z³,z⁴)`
3. **Accum pass** — reconstruct transmittance from moments + WBOIT-style weighted color
4. **Resolve** — composite onto opaque background

```
seta r_fbo 1
seta r_oit 2
vid_restart
```

Demo: `exec demo_mboit.cfg`. With mode 3: moments pass stays unlit; accum can use Forward+ tile lights on set 4 when `r_oitForwardPlus 1`.

Promotion to production is tracked in [OIT_FUTURE_TRACKS.md](OIT_FUTURE_TRACKS.md).

---

## Stochastic Alpha-Clipped Materials (`r_stochasticAlpha`)

Hashed / temporal alpha clip for **foliage, grates, hair cards, fabric holes, and decals** (shader `alphaFunc`). Not OIT.

| Mode | Behavior |
|------|----------|
| `0` | Hard `alphaFunc` discard (default) |
| `1` | Screen-space interleaved gradient noise hash |
| `2` | Temporal hash (requires TAA; falls back to 1 when off) |

```
seta r_stochasticAlpha 2
seta r_taa 1
```

No `vid_restart` required. Demo with mode 3 OIT: `exec demo_oit_clustered.cfg`.

## Related

- Renderer overview: [RENDERERS.md](RENDERERS.md)
- Unified Clustered transparent path: [UNIFIED_CLUSTERED_RENDERER.md](UNIFIED_CLUSTERED_RENDERER.md)
- Future tracks: [OIT_FUTURE_TRACKS.md](OIT_FUTURE_TRACKS.md)
- Spine 1.1 cert: [RENDERER_SPINE_1.1.md](RENDERER_SPINE_1.1.md)
