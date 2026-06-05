# Hybrid Rendering 1 (Granja / Pereira, 2021)

Implementation of the thesis **Hybrid-Rendering Techniques in GPU** (IST, July 2021): hybrid raster + 1-SPP ray tracing with a customized **SVGF** denoising stack for **separate shadow, indirect specular, and optional indirect diffuse** channels.

## Pipeline (per frame)

1. **G-buffer raster** — `r_renderMode 1`, `r_deferredGBuffer 1`, `r_deferredGBufferFill 1` (normals + roughness + albedo for adaptive blur and diffuse composite).
2. **Shadow RT** — 1-SPP sun visibility + shadow angle (`hybrid1_shadow.rgen`).
3. **Indirect specular RT** — 1-SPP glossy reflection (`hybrid1_spec.rgen`), optional **Reinhard** before denoise, **IBL** on miss/secondary hit via prefiltered cubemap (`r_hybrid1_ibl`).
4. **Indirect diffuse RT** (optional) — 1-SPP cosine hemisphere bounce (`hybrid1_diffuse.rgen`), **A-trous only** (no variance/temporal, per thesis §3).
5. **Temporal accumulation** (shadow + spec) — reprojection, variance estimate, **variance color clamping** (history rectification).
6. **Separable A-trous** — edge-avoiding 5-tap blur guided by depth, normals, luminance, variance; **adaptive start step** for roughness > 0.2 / shadow angle > 6°.
7. **Composite** — modulate raster HDR by denoised shadow; add denoised specular; add denoised diffuse × albedo.
8. **TAA** (optional) — post-process `r_taa` or auto via `r_hybrid1_taa 1` after composite.

RT **closest-hit** shaders reproject hit points into the deferred **G-buffer albedo** (when `r_deferredGBufferFill 1`) instead of flat placeholder colors.

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
r_hybrid1_diffuse 1       // indirect diffuse GI channel
r_hybrid1_ibl 1           // cubemap on RT miss / secondary hit (needs skybox or PBR cubemap)
r_hybrid1_taa 1           // TAA after hybrid composite (default on)
```

`r_rtx 1` **or** `r_hybrid1 1` before `vid_restart` enables KHR ray tracing device features.

## Cvars

| Cvar | Default | Role |
|------|---------|------|
| `r_hybrid1` | 0 | Master toggle (latched) |
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
| `r_hybrid1_shadowStrength` | 0.85 | Composite shadow weight |
| `r_hybrid1_specStrength` | 1.0 | Composite specular weight |
| `r_hybrid1_diffuseStrength` | 1.0 | Composite diffuse × albedo weight |
| `r_hybrid1_debug` | 0 | 1=shadow, 2=spec, 3=shadow angle, 4=diffuse |

When `r_hybrid1 1`, the legacy `r_rtx` demo composite pass is **skipped** (Hybrid1 owns RT output).

## Console commands

| Command | Role |
|---------|------|
| `hybrid1_status` | Print active state, channel toggles, denoise settings, composite weights |
| `hybrid1_reset` | Clear temporal history (also auto on resize, camera cut, or denoise cvar change) |
| `hybrid1_reload` | Rebuild Hybrid1 pipelines/images (after shader rebuild or when debugging init) |

## Troubleshooting

- **No RT output** — confirm RTX build (`./scripts/compile_engine.sh vulkan rtx`), `r_rtxDemo 1`, world loaded, and `[VK][Hybrid1] ... ready` in console after first frame.
- **Flat specular / no IBL** — enable skybox or PBR cubemap; `r_hybrid1_ibl 1`; check `hybrid1_status` for `ibl=1`.
- **Diffuse invisible** — `r_hybrid1_diffuse 1`, `r_deferredGBufferFill 1` (albedo for composite), try `r_hybrid1_debug 4`.
- **Ghosting after tuning denoise** — history resets automatically when denoise cvars change; run `hybrid1_reset` after large camera cuts.
- **Requires deferred raster first** — Hybrid1 composites onto the HDR color buffer after deferred lighting; enable `r_deferredLighting 1` with G-buffer fill.

Demo cfg: `exec demo_hybrid1.cfg` (enables diffuse, IBL, motion, TAA).

## Source layout

- Host: `src/renderers/vulkan/vk_hybrid1.c`
- Shaders: `src/renderers/vulkan/shaders/glsl/hybrid1/`
- SPIR-V embed: `vk_hybrid1_spirv.inc` (via `compile_shaders.sh`)
- Demo cfg: `examples/demo_game/mod/demo_hybrid1.cfg`

## References

Granja & Pereira, *Hybrid-Rendering Techniques in GPU*, IST 2021 — SVGF [Schied et al. 2017], variance color clamping [Salvi GDC 2016], separable A-trous [Benyoub / Digital Dragons].
