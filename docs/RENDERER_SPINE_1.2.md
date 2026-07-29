# Renderer Spine 1.2

**Status:** Foundation in progress — mode model + ownership gates landed; selective RT signal matrix and present-time adaptive recon are **not** fully certified.

Spine Tier A remains the certified production contract. Boot path:

`modern_vulkan.cfg` → [`modern_vulkan_stable.cfg`](../config/modern_vulkan_stable.cfg) → **Unified Clustered mode 3** + SMAA + GTAO + WBOIT, with TAA/RT off.

## Product tiers

| Tier | Mode | Role |
|------|------|------|
| **A — Certified Raster** | `r_renderMode 3` | Production spine (boot default): deferred opaque + clustered transparent/OIT |
| Forward+ legacy recovery | `r_renderMode 2` | Recovery / low-latency fallback |
| **B — Selective Hybrid** | `r_renderMode 4` | Clustered raster primary; exclusive RT signal owners |
| **C — Path-Traced Reference** | `r_renderMode 5` | Exclusive PT lighting (not an additive overlay) |

## Hard constraints (non-negotiable)

- **No frame generation**
- **No interpolated frame between simulation frames**
- **No intentional one-frame presentation latency**
- **Do not** turn all experimental systems on at once
- **Do not** replace the certified clustered raster spine without updating the mode contract
- **No** double lighting / double AO
- **No** RT requirement for normal gameplay
- **No** path-tracing overlay on top of completed raster lighting

`r_presentAdaptiveRecon` (default **0**) may only enable **same-frame** temporal recon / internal upscale paths. It must never insert FG or add display latency.

## Mode entry

```text
# Tier B
exec vulkan_overlay_selective_hybrid.cfg
vid_restart

# Tier C
exec vulkan_overlay_pt_reference.cfg
vid_restart

# Recovery (always)
exec modern_vulkan.cfg
# or
exec gfx_safe.cfg
```

## Ownership model

### Tier B (Selective Hybrid)

Rasterization remains the primary visibility and material path. Individual lighting signals may be replaced by ray-traced implementations (Hybrid1 channels today: shadow, specular; diffuse off by default in the selective overlay).

Each RT signal must have:

- one exclusive owner
- one raster / screen-space fallback
- independent history and denoise
- independent quality controls
- capability checks + failure recovery

Runtime demotion: **Hybrid1 + pathtrace** outside mode 5 → PT demoted (`hybrid1_x_pathtrace`).

### Tier C (Path-Traced Reference)

When `r_renderMode 5` + `r_pathtrace 1`:

- PT records **first** and owns HDR (`r_pathtrace_composite` latched to **1**)
- Hybrid1 is forced inactive
- Intended for material validation, lighting reference, screenshots, cinematic capture — not normal gameplay

## Present-time adaptive reconstruction

Same-frame presentation quality adaptation only:

| Allowed | Forbidden |
|---------|-----------|
| Temporal Reconstruction (`r_aaMode 4`) at sim frame rate | Frame generation |
| Internal temporal upscale (`r_upscale 2`) | Interpolated sim frames |
| Hybrid1 / PT channel denoise histories | Intentional +1 frame display lag |

## Static gates

```bash
./scripts/spine_1_2_mode_check.sh
./scripts/spine_combo_matrix_check.sh   # still requires stable mode 2
```

## Relation to Spine 1.1

Spine 1.1 cert (mode 3 + WBOIT + Temporal Reconstruction + weapon-after) remains opt-in and separate. See [RENDERER_SPINE_1.1.md](RENDERER_SPINE_1.1.md).

## Related

- [RENDERER_SPINE_1.0.md](RENDERER_SPINE_1.0.md) — certified boot
- [SELECTIVE_HYBRID_SHADOWS_1.0.md](SELECTIVE_HYBRID_SHADOWS_1.0.md) — exclusive RT sun shadows (GPU cert pending)
- [HYBRID_RENDERING1.md](HYBRID_RENDERING1.md) — Hybrid1 signal pipeline
- [PATHTRACE_ARCH_BENCHMARK.md](PATHTRACE_ARCH_BENCHMARK.md) — pathtrace scaffolding
- [UNIFIED_CLUSTERED_RENDERER.md](UNIFIED_CLUSTERED_RENDERER.md) — mode 3
