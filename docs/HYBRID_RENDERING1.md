# Hybrid Rendering 1 (Granja / Pereira, 2021)

Implementation of the thesis **Hybrid-Rendering Techniques in GPU** (IST, July 2021): hybrid raster + 1-SPP ray tracing with a customized **SVGF** denoising stack for **separate shadow, indirect specular, and optional indirect diffuse** channels.

## Pipeline (per frame)

1. **G-buffer raster** — `r_renderMode 1`, `r_deferredGBuffer 1`, `r_deferredGBufferFill 1` (normals + roughness + albedo for adaptive blur and diffuse composite).
2. **Shadow RT** — 1-SPP sun visibility + shadow angle (`hybrid1_shadow.rgen`).
3. **Indirect specular RT** — 1-SPP glossy reflection (`hybrid1_spec.rgen`), optional **Reinhard** before denoise, **IBL** on miss/secondary hit via prefiltered cubemap (`r_hybrid1_ibl`).
4. **Indirect diffuse RT** (optional) — 1-SPP cosine hemisphere bounce (`hybrid1_diffuse.rgen`), **A-trous only** (no variance/temporal, per thesis §3).
5. **Temporal accumulation** (shadow + spec) — reprojection, variance estimate, **variance color clamping** (history rectification).
6. **Separable A-trous** — edge-avoiding 5-tap blur guided by depth, normals, luminance, variance; **adaptive start step** for roughness > 0.2 / shadow angle > 6°.
7. **Composite** — modulate raster HDR by denoised shadow; add denoised specular; add denoised diffuse × albedo (or **Surfel irradiance × albedo** when `r_surfelGi_hybrid1Fusion 1` and Surfel GI is active).
8. **TAA** (optional) — post-process `r_taa` or auto via `r_hybrid1_taa 1` after composite.

RT closest-hit shaders prefer a **per-primitive world albedo SSBO** when `gl_InstanceCustomIndexEXT == 0`. With **`r_rtxWorldMaterials 1`** (default), world pack prefers diffuse **shader avgColor** (via **`lightingStage`** when set); with **`r_rtxWorldUvSample 1`** (default) it upgrades to **UV-centroid 8×8 diffuse thumbs** (fallback: avgColor → BSP vertex/face colors, including **SF_GRID**). **`r_rtxWorldAlbedoMode 1`** modulates material/UV × vertex color to keep lightmap bake in RT bounces (default **0** = replace). Entity hits (`customIndex == 1`) use parallel **entity albedo/normal SSBOs** (geo normals + per-surface albedo when **`r_rtxEntityMaterials 1`** — default). With **`r_rtxEntityUvSample 1`** (default), pack-time **UV-centroid samples** of 8×8 diffuse thumbs replace flat texture averages for MD3/IQM/MDR/glTF prims; otherwise shader/texture averages (or `refEntity.shader` tint / gray when materials are off). If that miss, they reproject hit points into the deferred **G-buffer albedo** (when `r_deferredGBufferFill 1`) instead of flat placeholder colors. True hit-shader texturing remains deferred.

Secondary rays also sample a packed **per-primitive world normal SSBO** (`hybrid1_sampleHitNormal`) for sun N·L, irradiance lookup, and specular IBL reflection — no ray-direction placeholders on the RTX path.

## Role vs `r_rtx` demo

**Hybrid1 is the production RT lighting path** (shadow + specular + optional diffuse + SVGF). The plain **`r_rtx` / `r_rtxDemo` overlay** remains a diagnostic / tint scaffold unless modes gain real visibility or reflection shading — keep Hybrid1 enabled for look-dev.

## Enable (RTX build)

```bash
./scripts/compile_engine.sh vulkan rtx   # USE_VULKAN_RTX=ON
```

In-game (latched — **`vid_restart`** after toggling):

```
r_fbo 1
r_hybrid1 1
r_rtxDemo 1          // world TLAS
r_renderMode 1
r_deferredGBuffer 1
r_deferredGBufferFill 1
vid_restart
```

Optional:

```
seta r_hybrid1Quality 2   // 0=custom, 1=performance, 2=balanced, 3=quality (live)
r_hybrid1_diffuse 1       // or use quality 3 instead of hand-tuning
r_hybrid1_ibl 1
r_hybrid1_taa 1
r_rtxEntities 1           // entity mesh BLAS: MD3 + CPU-skinned IQM/MDR + static/CPU-skinned glTF (+ AABB on pack fail)
r_rtxEntityTriCap 65536
```

`demo_hybrid1.cfg` sets `r_rtxEntities 1` and `r_hybrid1Quality 3`. Console **`rtx_status`** reports `entity_ents` / `entity_tris` / `mesh` breakdown (`md3` / `iqm` / `gltf` / `mdr`) / `proxy` reasons (`nonmesh` = unknown, `*fail` = pack failed → AABB), **entity BLAS mode** (`UPDATE` vs `REBUILD`), and **TLAS mode** (`UPDATE` vs `REBUILD` with reason).

`r_rtx 1` **or** `r_hybrid1 1` before `vid_restart` enables KHR ray tracing device features.

## Quality presets (`r_hybrid1Quality`)

| Value | Name | Effect (while `r_hybrid1` 1) |
|------:|------|------------------------------|
| 0 | custom | Leave individual `r_hybrid1_*` knobs alone |
| 1 | performance | No diffuse RT; 2 A-trous iters; hard sun; no dlight shadows |
| 2 | balanced | No diffuse; 3 A-trous; soft sun 0.25°; 1 dlight shadow |
| 3 | quality | Diffuse on; 4 A-trous; soft sun 0.5°; EnvBRDF IBL; 2 dlight shadows |

Live (no latch). Entity BLAS stays a separate latched companion (`r_rtxEntities` + `vid_restart`).

## Cvars

| Cvar | Default | Role |
|------|---------|------|
| `r_hybrid1` | 0 | Master toggle (latched) |
| `r_hybrid1Quality` | 0 | Preset tier (live; see table above) |
| `r_hybrid1_shadow` | 1 | Shadow trace + denoise |
| `r_hybrid1_spec` | 1 | Specular trace + denoise |
| `r_hybrid1_diffuse` | 0 | Indirect diffuse trace + A-trous denoise |
| `r_hybrid1_ibl` | 1 | Prefiltered/irradiance cubemap on RT miss and spec hits |
| `r_hybrid1_taa` | 1 | Enable post TAA when Hybrid1 active |
| `r_hybrid1_motion` | 1 | Motion-vector temporal reprojection for shadow/spec |
| `r_hybrid1_historyClamp` | 1 | Variance color clamping |
| `r_hybrid1_historyGamma` | 1.25 | Clamp box scale |
| `r_hybrid1_temporalAlpha` | 0.1 | Temporal blend to current sample |
| `r_hybrid1_adaptiveBlur` | 1 | Coarser first A-trous step on high noise |
| `r_hybrid1_separableBlur` | 1 | Horizontal + vertical passes |
| `r_hybrid1_reinhard` | 1 | Specular Reinhard pre/post denoise |
| `r_hybrid1_atrousIters` | 4 | Spatial filter iterations (0–4) |
| `r_hybrid1_phiColor` | 0.35 | A-trous luminance/color edge weight |
| `r_hybrid1_rayBias` | 0.02 | Normal offset for RT origins |
| `r_hybrid1_tMin` | 0.01 | Ray tMin for shadow/spec/diffuse |
| `r_hybrid1_depthTol` | 0.002 | A-trous depth edge stop |
| `r_hybrid1_normalDot` | 0.92 | A-trous minimum normal similarity |
| `r_hybrid1_adaptiveAngle` | 6 | Adaptive blur: shadow angle threshold (deg) |
| `r_hybrid1_adaptiveRough` | 0.2 | Adaptive blur: roughness threshold |
| `r_hybrid1_specRoughMax` | 0.98 | Skip specular RT at/above this roughness |
| `r_hybrid1_sunRadius` | 0 | Soft sun angular radius (degrees; 0=hard) |
| `r_hybrid1_contactHarden` | 1 | Shrink penumbra when N·L is high |
| `r_hybrid1_ggx` | 1 | GGX/VNDF specular + Fresnel |
| `r_hybrid1_iblMode` | 1 | 0=minimal, 1=prefilter, 2=split-sum EnvBRDF (`vk.brdflut`) |
| `r_hybrid1_diffuseDirect` | 1 | Sun+irradiance on diffuse secondary hits |
| `r_hybrid1_dlightShadows` | 0 | Top-N Forward+ dlight RT shadows (1–4); UBO fallback if FP off |
| `r_hybrid1_shadowStrength` | 0.85 | Composite shadow weight |
| `r_hybrid1_specStrength` | 1.0 | Composite specular weight |
| `r_hybrid1_diffuseStrength` | 1.0 | Composite diffuse × albedo weight |
| `r_hybrid1_debug` | 0 | 1=shadow, 2=spec, 3=shadow angle, 4=diffuse, 5=Surfel irradiance |

### Surfel GI fusion

When **`r_surfelGi 1`** and **`r_surfelGi_hybrid1Fusion 1`** (default), Hybrid1 skips diffuse RT and composites Surfel screen irradiance instead — see `docs/SURFEL_GI.md`. `hybrid1_status` reports `surfelFusion=1`. Set `r_surfelGi_hybrid1Fusion 0` to restore independent Surfel scene composite (can double-add if both on).

When `r_hybrid1 1`, the legacy `r_rtx` demo composite pass is **skipped** (Hybrid1 owns RT output).

## Console commands

| Command | Role |
|---------|------|
| `hybrid1_status` | Print active state, readiness reason, shared TLAS state, channel toggles, denoise settings, composite weights |
| `hybrid1_reset` | Clear temporal history (also auto on resize, camera cut, or denoise cvar change) |
| `hybrid1_reload` | Rebuild Hybrid1 pipelines/images (after shader rebuild or when debugging init) |

`hybrid1_status` reports concise states such as `ready`, `blocked: r_fbo 1 is required`, `blocked: r_rtxDemo 1 is required for shared TLAS`, `waiting: shared RTX TLAS not ready`, or `ready: all trace channels disabled`.

## Troubleshooting

- **No RT output** — confirm RTX build (`./scripts/compile_engine.sh vulkan rtx`), `r_rtxDemo 1`, world loaded, and `hybrid1_status` reports `ready` after first frame.
- **Flat specular / no IBL** — enable skybox or PBR cubemap; `r_hybrid1_ibl 1`; check `hybrid1_status` for `ibl=1`.
- **Diffuse invisible** — `r_hybrid1_diffuse 1`, `r_deferredGBufferFill 1` (albedo for composite), try `r_hybrid1_debug 4`.
- **Ghosting after tuning denoise** — history resets automatically when denoise cvars change; run `hybrid1_reset` after large camera cuts.
- **Requires deferred raster first** — Hybrid1 composites onto the HDR color buffer after deferred lighting; enable `r_deferredLighting 1` with G-buffer fill.

Demo cfg: `exec demo_hybrid1.cfg` (enables diffuse, IBL, motion, TAA).

## Source layout

- Host: `renderers/vulkan/extensions/rtx/vk_hybrid1.c`
- Shared TLAS/BLAS: `renderers/vulkan/extensions/rtx/vk_rtx.c`, `vk_rtx_entities.c`
- Shaders: `renderers/vulkan/shaders/glsl/hybrid1/`
- SPIR-V embed: `vk_hybrid1_spirv.inc` (via `compile_shaders.sh`)
- Demo cfg: `examples/demo_game/mod/demo_hybrid1.cfg`
- Overlay: `config/vulkan_overlay_hybrid1.cfg`

## References

Granja & Pereira, *Hybrid-Rendering Techniques in GPU*, IST 2021 — SVGF [Schied et al. 2017], variance color clamping [Salvi GDC 2016], separable A-trous [Benyoub / Digital Dragons].
