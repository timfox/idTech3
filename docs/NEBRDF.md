# Neural Enhancement of Analytical Appearance Models

Shen, Ma, Zhou & Wu, *Neural Enhancement of Analytical Appearance Models* ([arXiv:2604.24081](https://arxiv.org/abs/2604.24081), 2026).

This module is a **calibrated research scaffold**: GGX computational-graph hats, hypercube search summary, MLP/param sizes, and paper fit/render metrics. It does **not** ship trained MLP weights or replace shipping GGX in `gen_frag.tmpl`.

## Toggle

| Cvar | Default | Purpose |
|------|---------|---------|
| `cl_nebrdf_enable` | `0` | Gate console commands; startup log when research profile is built |

## Build

```bash
./scripts/compile_engine.sh vulkan full    # or research
ctest -R unit_nebrdf -V
./tests/scripts/test_nebrdf.sh build-vk-Release
```

Requires **`USE_RESEARCH_EXTENSIONS=ON`**.

## Model mapping (paper → engine)

| Paper | Engine |
|-------|--------|
| GGX graph nodes M,S,D,F,G,1/E + ops (§3, Fig. 2) | `NeBrdf_GetNode` / `NeBrdf_NodeCount` (N=11) |
| Final enhanced \(M+S\cdot D\cdot\hat{F}\hat{\cdot}\hat{G}\cdot\hat{(1/E)}\) | `NeBrdf_FinalFormula` / `NeBrdf_IsNeural` |
| Fig. 1 order E → G → mul(F×G) → F | `NeBrdf_EnhancementOrder` |
| Hypercube Hamming ≤1 → N+1 (§4) | `NeBrdf_HypercubeNeighbors` |
| 12 + 27 = 39 params; MLP {16,32,16}; ~26.45 KB | `NeBrdf_ParamCounts` |
| Fit 27.3s / 34.2s; rays/s 13.68M / 21.83M (§7) | `NeBrdf_FitTimeSec` / `NeBrdf_RenderRaysPerSec` |
| Fig. 5 SSIM subset | `NeBrdf_CompareRow` |

## Console

```
set cl_nebrdf_enable 1
nebrdf_paper
nebrdf_graph
nebrdf_search
nebrdf_mlp
nebrdf_perf
nebrdf_compare
nebrdf_advice fit
nebrdf_status
```

## Demo

```
exec demo_nebrdf.cfg
```

## Validation

`unit_nebrdf` checks: F/G/1E neural; order length 4; params 12+27=39; Hamming-1 = N+1; enhanced fit time &lt; GGX; advice non-empty.

## Limitations

- No MERL/OpenSVBRDF training or weight blobs in the build
- Shipping Vulkan PBR remains analytic GGX (`D_GGX`, Schlick F, Smith G)
- No change to Unified Clustered / Forward+ lighting ownership

## Follow-up

Weight pack + GLSL MLP eval + `r_nebrdf` latch (optional).

## Related

- Engine GGX: [PBR_TEXTURES.md](PBR_TEXTURES.md), `gen_frag.tmpl`
- Measured black materials scaffold: [HOWDARK.md](HOWDARK.md)
