# SqueezeMe — mobile Gaussian full-body avatars (experimental)

Implementation scaffold for [SqueezeMe: Mobile-Ready Distillation of Gaussian Full-Body Avatars](https://arxiv.org/abs/2412.15171v4) (Iandola et al., Meta Codec Avatars Lab, 2025).

The paper trains a UV-mapped 3D Gaussian avatar with a CNN pose decoder, then **distills** pose correctives into **linear layers** and applies **Gaussian corrective sharing (GCS)** so inference fits mobile VR (Quest 3 class: **3 avatars @ 72 FPS** with a custom Vulkan splat path).

This fork provides:

| Stage | Status |
|-------|--------|
| UV GCS layout + linear correctives (CPU) | In-engine (`vk_squeezeme.c`) |
| LBS + template Gaussians | Simplified 12-joint demo |
| Vulkan splat composite | Reuses [Mobile-GS](MOBILE_GAUSSIAN_SPLATTING.md) (`mgs_*` shaders) |
| CNN training / dome capture | Out-of-tree (PyTorch); use your AG/Animatable-Gaussians exports |
| PCA distillation | `scripts/squeezeme_distill.py` |
| Demo `.sqz` pack | `scripts/sqz_pack_demo.py` |

## Enable

```text
r_squeezeme 1
r_squeezeme_tier 2          // Mobile-GS splat tier when r_mgs=0
r_squeezeme_avatars 3       // paper target: 3 concurrent avatars
r_squeezeme_gcs 1           // 64² shared correctives, 4× nearest upsample
r_squeezeme_linear 1        // distilled linear decoder (DL_GCS)
vid_restart
```

Requires **`r_fbo 1`** (same as Mobile-GS). `r_squeezeme` drives the splat tier via `R_MGS_EffectiveTier()` even when `r_mgs` is 0.

## Console

- `sqz_status` — avatar count, gaussian total, GCS/linear flags

## Asset format (`.sqz` v1)

Binary layout (little-endian): magic `SQZ1`, header fields, `poseMean`, `poseBasis`, `correctivesBasis` (Algorithm 1), per-cell `template`, `mask`, `jointWeights`, `bindPos`. See `src/renderers/vulkan/sqz_format.h`.

Default search path: `sqz/demo.sqz` (generated into the demo pk3).

## Offline pipeline

1. Train high-fidelity UV Gaussian avatar + CNN decoder (paper §4.1; external).
2. Export keyframe poses + `M(D(p))` corrective maps.
3. Run distillation:

```bash
python3 scripts/squeezeme_distill.py \
  --poses training/poses.npy \
  --correctives training/corr.npy \
  --out sqz/subject.sqz
```

4. Ship `.sqz` in mod pk3; set `r_squeezeme_asset sqz/subject.sqz`.

Pack a minimal demo blob:

```bash
python3 scripts/sqz_pack_demo.py examples/demo_game/mod/sqz/demo.sqz
```

## Paper mapping

| Paper | Engine |
|-------|--------|
| CNN decoder `D(ef, eb)` | Training-time only |
| Linear `D_L(p)` + PCA | `SQZ_EvalLinearCorrectives` |
| GCS 64² → 256² nearest | `SQZ_UpsampleGCSNearest` |
| `G = LBS(Gt + M(U(D_L(p))))` | `SQZ_BakeAvatar` |
| Vulkan splat + stereo cull | Mobile-GS prepare/splat/composite |

## Limitations (v1 scaffold)

- Procedural/demo avatar when `.sqz` missing; not dome-trained identity quality.
- CPU bake + linear layer (no Quest NPU path yet).
- Isotropic Mobile-GS splats (not full 3D covariance rasterizer from paper Fig. 3).
- No multi-view training loss; demo pose is sinusoidal joint motion.

## See also

- [MOBILE_GAUSSIAN_SPLATTING.md](MOBILE_GAUSSIAN_SPLATTING.md)
- [GAUSSIAN_RAY_TRACING_GRTX.md](GAUSSIAN_RAY_TRACING_GRTX.md)
- [RENDERERS_FUTURE.md](RENDERERS_FUTURE.md)
- Paper demo: https://forresti.github.io/squeezeme
