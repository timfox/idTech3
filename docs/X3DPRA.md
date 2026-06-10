# x3DPRA — 3D Extended Phaseless Rytov Approximation

Device-free RF volumetric imaging from phaseless RSS measurements (Ma et al., [arXiv:2606.06933](https://arxiv.org/abs/2606.06933)).

## Overview

| Component | Description |
|-----------|-------------|
| **x3DPRA** | 3D extension of xPRA: linear phaseless model + background subtraction + sparse Fresnel mask |
| **Sensing** | 2.4 GHz Wi-Fi RSS, vertically polarized dipoles, no phase/coherent sync required |
| **DOI** | 0.9 × 0.9 × 0.3 m³, 60×60×15 voxels |
| **Optimization** | min ½‖y − WΔα‖² + γ TV₃D(Δα) with Huber TVReg |

## Engine (C)

Compiled into `qcommon`:

- Attenuation/contrast from permittivity (Eq. 1, 12)
- Kernel ψ and Fresnel ellipse mask (Eq. 19, 26)
- Ground-truth scenes from Section V (circle, square, two cylinders)
- 3D Huber TV helper

**Cvars**

| Cvar | Default | Role |
|------|---------|------|
| `cl_x3dpra_enable` | `1` | Enable console + startup log |
| `cl_x3dpra_repo` | `""` | Repo root for Python tools |
| `cl_x3dpra_python` | `python3` | Python interpreter |
| `cl_x3dpra_fresnel_delta` | `0.2` | Ellipse width Δd (m) for sparse W |

**Commands**

```
x3dpra_info
x3dpra_scene
x3dpra_kernel_test
x3dpra_reconstruct [--object circle|square|two_cylinders] [--mode 2d|3d]
```

## Python pipeline

```bash
cd tools/x3dpra
pip install -r requirements.txt

python reconstruct.py --dry-run
python reconstruct.py --object circle --mode 3d --output circle.npy
python reconstruct.py --object square --mode 3d
python reconstruct.py --object two_cylinders --mode 3d
```

### Model (paper §III–IV)

**Phaseless forward model (after background subtraction):**

```
y = W Δα + n,   W = Im(G)/k0 ⊙ S
Δα_n = -k0 Im(ΔχRI(r_n))
```

**Extended contrast (low-loss):**

```
χRI = 2(√εR − 1) + j εI/√εR
```

**Optimization:**

```
Δα̂ = argmin ½‖y − WΔα‖² + γ T3D(Δα)
```

T3D uses discrete 3D gradients with Huber penalty (Eq. 29–31).

### Simulation objects (§V)

| Object | εr | α | Notes |
|--------|-----|------|-------|
| Circular cylinder | 10+1j | 15.8 | Ø0.3 m, h0.8 m |
| Square cylinder | 8+0.8j | 14.2 | 0.36 m sides |
| Two cylinders | 15+1.5j | 19.5 | Ø0.4 m, separated in z |

## Tests

```bash
cmake --build build-vk-Release --target unit_x3dpra
./build-vk-Release/unit_x3dpra
```

## API (C)

```c
#include "x3dpra/x3dpra.h"

float alpha = X3dpra_AttenuationFromPermittivity(10.0f, 1.0f);
float w = X3dpra_WeightEntry(psi_im, X3dpra_Wavenumber());
qboolean in = X3dpra_FresnelMask(r_mt_n, r_mr_n, r_mt_mr, 0.2f);
```

## Notes

- Full electromagnetic validation uses CST Studio Suite in the paper; this repo provides the x3DPRA formulation, sparse forward model, and TVReg-style solver.
- For experimental RSS data, supply link geometry + ΔP measurements and call `build_weight_matrix()` with your node/voxel layout.

## Data & weights

No pretrained weights. Experimental RSS: nodes CSV + NPZ measurements via `tools/x3dpra/io.py` and `reconstruct.py --nodes --measurements [--background]`. CST surrogate fixture optional (`tools/x3dpra/fixtures/`). PSNR regression bands: `tools/x3dpra/benchmarks/psnr_targets.json`.
