# Variable-Rate Compute Shading (VRCS)

Chocolate path that shades a subset of deferred-lighting pixels (primaries) and copies results to neighbors (duplicates), then deblocks. Pure compute — no hardware fragment shading rate.

## Requirements

- `r_fbo 1`, deferred lighting on (`r_renderMode` 1 or 3 with `r_deferredLighting 1`)
- Build: `./scripts/compile_engine.sh vulkan`

## Enable

```
exec demo_vrcs.cfg
vid_restart
```

Or:

```
seta r_fbo 1
seta r_renderMode 3
seta r_deferredLighting 1
seta r_vrcs 1
vid_restart
```

Console: **`vrcs_status`**.

`vrcs_status` reports the activation reason (`ready`, `r_vrcs_off`, `deferred_lighting_inactive`, missing shader module names, or missing runtime resources) plus the current Forward+/deferred target/resource readiness. This is the quickest way to tell whether VRCS is actually dispatching or whether the normal deferred lighting fallback is still in use.

## Cvars

| Cvar | Default | Role |
|------|---------|------|
| `r_vrcs` | 0 | Master (latched) |
| `r_vrcs_quality` | 1 | 0=aggressive, 1=balanced, 2=quality |
| `r_vrcs_extraHalf` | 1 | Prefer half-rate over pure 2×2 (noise) |
| `r_vrcs_deblock` | 1 | Neighbor average after lighting |
| `r_vrcs_debug` | 0 | 0=off, 1=SRI rates, 2=primary/dup, 3=packed tint |
| `r_vrcs_edge` | 0.08 | Luma gradient → 1×1 |
| `r_vrcs_flat` | 0.02 | Below this → allow 2×2 |

## Pipeline

1. **SRI** — half-res `R8_UINT` rates from G-buffer albedo luma gradients (Sobel-style)
2. **Pack** — per 16×16 tile: primary count, flags (`force 1×1`, `all 2×2`, `sky`), packed XY + copy bits
3. **Lighting** — 256-thread workgroups; primaries shade via Forward+ tile lights; duplicates early-out; copies written in postfix
4. **Deblock** — average H/V/2×2 neighborhoods (in-place UAV)
5. Existing deferred **graphics composite** unchanged

## Notes

- When packing cannot retire ≥1 wave of 32, the tile forces full 1×1 rate.
- Deblocker intentionally spills across triangle edges; TAA absorbs most silhouette error.
- If any required shader module or deferred/Forward+ runtime resource is unavailable, the engine falls back to the standard deferred lighting compute path.
- References (public): Martin Fuller, “Variable Rate Compute Shaders”; Michal Drobot, software VRS deblocker (SIGGRAPH 2020).

## Files

- Host: `renderers/vulkan/vk_vrcs.c`
- Shaders: `renderers/vulkan/shaders/glsl/vrcs/`, `deferred_lighting_vrcs.comp`
