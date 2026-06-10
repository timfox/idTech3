# RadiusFPS

Efficient **farthest point sampling (FPS)** via spherical voxel pruning (Yu et al., [arXiv:2606.06255](https://arxiv.org/abs/2606.06255)).

## Overview

RadiusFPS accelerates exact FPS while preserving the standard distance-update rule under a fixed seed and deterministic tie-breaking (smallest original point index on equal distance).

Two implementations ship in the engine:

| Variant | Description |
|---------|-------------|
| **RadiusFPS (CPU)** | Active-voxel index, spherical radius pruning (Eq. 25), coordinate-wise point skip (Eq. 28) |
| **RadiusFPS-G (GPU)** | CUDA fusion kernels: init + hierarchical selection + per-voxel filter/update |

## Build

CPU RadiusFPS is always compiled into `qcommon`.

Optional GPU path (Linux + NVIDIA CUDA toolkit):

```bash
cmake -DUSE_RADIUSFPS_CUDA=ON ...
# or extend compile_engine.sh as needed
```

## Console

| Cvar | Default | Meaning |
|------|---------|---------|
| `cl_radiusfps_enable` | `1` | Log startup + allow self-test command |
| `cl_radiusfps_nvox` | `16` | Voxel bins per axis (paper parameter *v*) |
| `cl_radiusfps_backend` | `cpu` | `cpu`, `gpu`, or `ref` |
| `cl_radiusfps_seed` | `1` | Initial sample seed |

Commands:

```
radiusfps_info
radiusfps_sample [num_points] [num_samples]
```

`radiusfps_sample` compares output against vanilla FPS and prints timing.

## API

```c
#include "radiusfps/radiusfps.h"

radiusfps_config_t cfg;
RadiusFPS_DefaultConfig(&cfg);
cfg.nvox = 16;
cfg.seed = 1;
cfg.backend = RADIUSFPS_BACKEND_CPU;

int *indices = ...; /* num_samples ints */
RadiusFPS_Sample(points_xyz, num_points, num_samples, indices, &cfg, NULL, NULL);
```

`points_xyz` is interleaved `[x0,y0,z0, x1,y1,z1, ...]`.

Reuse voxelization for repeated sampling on the same cloud:

```c
radiusfps_workspace_t *ws = NULL;
RadiusFPS_BuildWorkspace(points_xyz, n, &cfg, &ws);
RadiusFPS_Sample(points_xyz, n, m, indices, &cfg, ws, NULL);
RadiusFPS_FreeWorkspace(ws);
```

## Tests

```bash
cmake --build build-vk-Release --target unit_radiusfps
./build-vk-Release/unit_radiusfps
# or
./tests/scripts/test_radiusfps.sh
```

## References

Yu, Li, Chang, Miyazaki — *RadiusFPS: Efficient Farthest Point Sampling on CPUs and GPUs via Spherical Voxel Pruning*, arXiv:2606.06255, 2026.

## Data & weights

No external weights. Optional CUDA build: `-DUSE_RADIUSFPS_CUDA=ON` (Linux + NVIDIA toolkit). Paper-scale point clouds can be loaded via PLY in `tools/radiusfps/benchmark.py` (optional). GPU workspace reuse: build CPU `radiusfps_workspace_t` once, then call `RadiusFPS_Sample` with the same workspace pointer for repeated sampling on an unchanged cloud.
