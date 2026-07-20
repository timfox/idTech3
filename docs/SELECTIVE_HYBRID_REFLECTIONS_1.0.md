# Selective Hybrid Reflections 1.0

**Status:** Implementation landed — **GPU certification pending** (static gates only).

Does **not** change the certified Forward+ Spine 1.0 boot path or `modern_vulkan.cfg`.

## Ownership

| Layer | Behavior |
|-------|----------|
| Subsystem | `REFLECTION_SPECULAR` — exclusive owner of final indirect specular |
| Effective owners | `off` / `probe` / `ssr` / `hybrid1_rt` / `path_tracer` |
| Per-pixel (RT healthy) | RT hit → Hybrid1 miss/probe (same buffer); SSR pass **off** |
| Demotion waterfall | RT unhealthy → SSR → probe/sky |

**Never** add RT + SSR + probe at full strength.

Normalized conceptual weights (Stage B global demotion + RT-buffer packing):

```text
rtWeight   = validatedRtHitConfidence   // packed A >= 0.5
ssrWeight  = (1 - rtWeight) * ssrConf   // only when owner == ssr
probeWeight = 1 - rtWeight - ssrWeight  // miss / rough / demoted
```

When `hybrid1_rt` owns: Hybrid1 miss and roughness-skip sample prefiltered env (terminal probe in-buffer). `gen_frag` IBL specular is suppressed (`pbrDebugMode.w`).

## Target configuration

| Signal | Owner |
|--------|-------|
| Primary visibility | Raster (clustered / mode 3–4) |
| Clustered direct lighting | Raster |
| Shadows | Raster or Selective Hybrid Shadows (independent) |
| **Indirect specular** | **SHR waterfall** |
| AO | GTAO — **not** multiplied onto valid RT reflection |
| GI / lightmaps | Lightmaps + SH/IBL |
| Transparency | Optional WBOIT; RT reflections opaque-only |
| AA | SMAA |
| Weapon | Outside world reflection history |
| UI | After tonemap |

Out of scope: diffuse RT GI, path-traced primary, frame generation, visibility-buffer shading, neural rendering.

## Candidate representation

| Channel | Layout |
|---------|--------|
| Hybrid1 `spec` RGBA16F | RGB = linear HDR reflection; A = packed roughness + hit bit (`[0,0.5)` miss/probe, `[0.5,1]` RT hit) |
| SSR `ssr_image` | RGB = blended scene; A = hit confidence |
| Probe | Hybrid1 miss / rough-skip prefilter, or gen_frag IBL when owner=`probe` |

## Hit shading (one-bounce, not path-traced)

Hybrid1 `hybrid1_spec.rchit`:

- Hit albedo (bindless / albedo tex)
- Optional secondary IBL on hit (`r_hybrid1_ibl`)
- No recursive specular bounce
- Miss: prefiltered environment along reflection direction

## Roughness policy

- `r_shrRoughnessRtMax` (default **0.55**): above → skip RT, sample probe, miss packing
- Mirror/smooth: RT preferred
- Rough: probe preferred

## Denoising

Dedicated Hybrid1 specular history (`hist_spec` / `var_spec`) — **not** world `taa_history`.

When SHR owns (`shsMode==2` on temporal):

- Motion / OOB rejection
- Normal discontinuity
- Hit↔miss source transition rejection
- Finite max age (`r_shrMaxHistoryAge`)
- Alpha floor (`r_shrTemporalAlphaFloor`)

Existing A-trous spatial filter; roughness unpacked from packed A.

## AO / specular occlusion

- Valid RT hit: **no** scalar GTAO/AV on specular add
- Probe/miss: optional `r_shrProbeSpecOcclusion` × AV

## Transparent / weapon / portals

- Opaque world only for RT reflections
- Weapon: Forward+ / probe; does not write Hybrid1 world history
- Portal/mirror: inherit Hybrid1 temporal lifecycle resets; dedicated mirror path preferred when present

## Fail injection (`r_shrFailInject`)

| Bit | Failure |
|----:|---------|
| 1 | TLAS |
| 2 | RT pipeline |
| 4 | Descriptor |
| 8 | History |
| 16 | SSR |

Expected: presentable frame, demote to SSR or probe, status reports reason, no restart loop.

## Debug (`r_shrDebug`)

Mapped into Hybrid1 composite (see `vk_shr_composite_debug_mode`). Key modes: RT radiance (2), source select (14), RT confidence (13), history weight/reject (10/11).

## Enable

```text
exec vulkan_overlay_selective_hybrid_reflections.cfg
vid_restart
```

Recovery: `exec modern_vulkan.cfg`

Static gate: `./scripts/selective_hybrid_reflections_1_0_check.sh`

## Certification stages

| Stage | Scope | Status |
|-------|-------|--------|
| A | Raw RT + probe fallback, no denoise claim | Static only |
| B | Exclusive routing, no dual energy | Static only |
| C | Dedicated history / rejection | Static only |
| D | Lifecycle / soak / fail inject | **Not run** |

## Remaining gaps (ranked)

1. Per-pixel RT→SSR→probe resolve while RT globally healthy (SSR currently demoted only)
2. Material parity (clearcoat / transmission / anisotropy) beyond opaque dielectric/metal
3. Transparent / OIT RT reflections
4. Portal/mirror view-family histories
5. GPU soak metrics (coverage %, timings)

Alpha-tested specular any-hit (`hybrid1_spec.rahit`) is wired for Stage A geometry holes (fences/foliage).
