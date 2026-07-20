# Moment-Based OIT & Stochastic Alpha

## Moment Transparency / MBOIT (`r_oit 2`)

Order-independent transparency for **glass, smoke, particles, translucent surfaces, and overlapping transparent layers**.

| Mode | Technique |
|------|-----------|
| `r_oit 0` | Off (sorted alpha blend) |
| `r_oit 1` | WBOIT (weighted blended) |
| `r_oit 2` | **MBOIT / Moment Transparency** (Sharpe HPG 2018 / Münstermann I3D 2018 style) |

Requires `r_fbo 1` and `vid_restart`.

### Algorithm (mode 2)

1. Opaque geometry (depth write)
2. **Moments pass** — additive optical depth `d = -log(1-α)` and power moments `d·(z,z²,z³,z⁴)`
3. **Accum pass** — reconstruct transmittance `T(z)` from moments (Cantelli/MSM-style + β=0.25 overestimation), WBOIT-style weighted color + revealage
4. **Resolve** — composite onto opaque background

```
seta r_fbo 1
seta r_oit 2
vid_restart
```

Demo: `exec demo_mboit.cfg` (or `vulkan_overlay_mboit.cfg`).

### With Unified Clustered (`r_renderMode 3`)

Pair OIT with deferred opaque. Both WBOIT and MBOIT accum use Forward+ tile lights via `r_oitForwardPlus 1` (default; MBOIT moments pass stays unlit).

`r_oitClassify 1` splits transparent draws into alpha-blend (MBOIT/WBOIT) vs additive particles/smoke (WBOIT, no moments), compositing additive last. Default `0` keeps a single global bucket. Hair cards stay on `r_stochasticAlpha`, not OIT.

```
exec vulkan_overlay_oit_clustered.cfg
vid_restart
```

Demo: `exec demo_oit_clustered.cfg`. See [UNIFIED_CLUSTERED_RENDERER.md](UNIFIED_CLUSTERED_RENDERER.md). Set `r_oitForwardPlus 0` to restore unlit MBOIT/WBOIT accum.

### Temporal Reconstruction

When `r_taa` / `r_aaMode` 4–5 is active, OIT revealage stamps a dedicated R8 **reactive mask** (not `oit_reveal` itself) so Temporal Reconstruction prefers the current frame on glass/smoke (`r_temporalReactiveMask 1`). Forward+ transparent and stochastic survivors also stamp via `gen_frag`. Raw OIT accum/reveal are never temporally blended — resolve into HDR first via `texelFetch` + NEAREST with an explicit **COLOR_ATTACHMENT → SHADER_READ_ONLY** full-framebuffer barrier after accum (prevents horizontal scanline tears / stipple from BY_REGION races and same-layout barriers). WBOIT weights use the McGuire/Bavoil reversed-Z–adapted curve clamped to `[1e-2, 3e3]` so fp16 underflow cannot paint doorway stipple bands. First-person weapons are deferred past world TAA (`r_temporalWeaponAfterTaa 1`). See [HDR_GAPS.md](HDR_GAPS.md) §6.8.

### Resolve equation (WBOIT / MBOIT accum)

McGuire/Bavoil composite (linear HDR):

```
C_avg = accum.rgb / max(accum.a, eps)
C_out = C_avg * (1 - revealage) + C_bg * revealage
```

Clears: accum `vec4(0)`, revealage `1`. Depth test uses reversed-Z `GREATER_OR_EQUAL` (no depth write). Debug: `r_oitDebug` 1–13; NaN/Inf / cluster OOB → magenta. Console: `oit_status`.

### Glyph / block corruption fix (resolve FB ownership)

**Root cause (earlier):** `oit_resolve` framebuffer creation reused `attachmentCount` 2/3 from the accum/moments FB setup while the resolve render pass has a single color attachment. On drivers that still create the FB, resolve could sample/write wrong attachment identity — repeated block/glyph-like patterns across HDR (including under the first-person weapon).

**Fixes:** force `attachmentCount = 1` for resolve; `oitAttachmentGeneration` / `oitDescriptorGeneration` must match before accum/resolve (else skip OIT); non-MSAA depth restored to `DEPTH_STENCIL_ATTACHMENT_OPTIMAL` before accum; Forward+ tile reads bounds-checked.

### Rectangular / tile-band corruption fix (resolve layout lifecycle)

**Root cause:** Resolve used `initialLayout=COLOR_ATTACHMENT` + `loadOp=DONT_CARE` after an explicit `SHADER_READ→COLOR_ATTACHMENT` transition. When that old-layout assumption was wrong (classify mid-loop fog_scene copy, deferred/MSAA residuals), DONT_CARE left **undefined tile memory** in horizontal bands / rectangular blocks. The weapon region showed the same corrupted HDR because resolve runs before weapon.

**Fixes (WBOIT-first):**
- Resolve RP: `initialLayout=UNDEFINED`, `loadOp=DONT_CARE`, `finalLayout=SHADER_READ_ONLY` (fullscreen rewrite; no fragile pre-transition)
- Single `oitAttachmentGeneration` bump after OIT FBs exist; descriptors must match
- `oitFrameState` UNTOUCHED→CLEARED→ACCUMULATED→RESOLVED; refuse resolve from UNTOUCHED
- Reactive reveal stamp once after final classify bucket (not between buckets)
- Extent triad check (oitExtent vs render vs mainColor) before resolve
- Weapon flush asserts OIT flags cleared
- `r_oitDirectTest 1` clears+resolves without transparent draws
- `oit_capture` / expanded `oit_status` FrameContext (frame, cmdIndex, swapchainImage independent)

**Pass order (world):** opaque → deferred → OIT accum → OIT resolve → refractive (water/glass when `r_refractiveExcludeOit 1`) → weapon (`RDF_NOWORLDMODEL`) → post → UI. Weapon never writes world OIT targets.

**Repro / isolation:** `./scripts/repro_oit_corruption.sh` or `exec repro_oit_corruption.cfg` after Ultra/FA.

**First corrupt producer (code + isolation):** `oit_resolve` writing `color_image` under `DONT_CARE` with a wrong prior layout — not accum/reveal contents when cleared. Weapon contamination = same resolved HDR under the gun.

**Isolation matrix (device; stop at first band/tile failure):**

| Step | Setting | Expected if fix holds |
|------|---------|------------------------|
| B0 | `r_oit 0` | Clean opaque/weapon/UI |
| B1 | WBOIT raw (`r_oitForwardPlus 0`, `r_oitClassify 0`) | No rectangular bands |
| B2 | `r_oitClassify 1` | No mid-bucket bands |
| B3 | `r_oitForwardPlus 1` | No magenta tile slabs (OOB) |
| B4 | `r_oitDebug` 1–13 | Stage views coherent |
| B5 | `r_oitDebug 14` | Magenta×coverage only (ignore accum RGB) |
| B5b | `r_oitDebug 15` | Smooth FragCoord UV (no bands) |
| B6 | `r_oitDirectTest 1` | Pure opaque after clear |
| B6b | `r_oitDirectTest 2` | Smooth half UV-gradient composite |
| B7 | `cg_drawGun 0/1` | Gun clean when world resolve clean |

**Odd extents (device):** native + 1279×719 / 1365×767 / 1601×901 (`r_renderWidth`/`r_renderHeight` or window resize). Lifecycle: `vid_restart`, map change. No invented soak timings — report measured duration only if run.

Commands: `oit_status` / `oit_capture stages`. Static gate: `./scripts/oit_corruption_check.sh`.

### Promotion (after this fix)

| Capability | Class |
|------------|--------|
| WBOIT unlit | quality opt-in (lifecycle hardened; live soak pending device run) |
| WBOIT Forward+ | quality opt-in |
| WBOIT classify buckets | quality opt-in |
| MBOIT | experimental until WBOIT live matrix passes |
| Weapon interaction | quality opt-in (resolve-before-weapon + state clear) |
| Boot `modern_vulkan.cfg` | unchanged certified fallback |

## Stochastic Alpha-Clipped Materials (`r_stochasticAlpha`)

Hashed / temporal alpha clip for **foliage, grates, hair cards, fabric holes, and decals** (shader `alphaFunc`).

| Mode | Behavior |
|------|----------|
| `0` | Hard `alphaFunc` discard (default) |
| `1` | Screen-space interleaved gradient noise hash |
| `2` | Temporal hash (frame-seeded; requires Temporal Reconstruction — auto-falls back to mode 1 when TAA is off) |

```
seta r_stochasticAlpha 2
seta r_taa 1
```

No `vid_restart` required (push-constant driven). Applies to `GT0` / `LT128` / `GE128`. If mode 2 is set while `r_taa` is off, the backend pushes mode 1 so coverage is not frozen without history.

## Related

- Existing WBOIT: [RENDERERS.md](RENDERERS.md)
- Unified Clustered transparent path: [UNIFIED_CLUSTERED_RENDERER.md](UNIFIED_CLUSTERED_RENDERER.md)
