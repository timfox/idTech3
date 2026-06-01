# Caspar (SymForce experimental)

[Caspar](https://github.com/symforce-org/symforce/tree/v0.10.1/symforce/experimental/caspar) is a **CUDA accelerator for symbolic programming with adaptive reordering** (Martens et al., arXiv:2605.30583). It ships inside **SymForce** as `symforce.experimental.caspar` — this engine repo does **not** reimplement Caspar; we provide optional checkout, build helpers, and validation scripts.

## What Caspar provides

- **Symbolic → CUDA**: DABSEG graph, hardware-mapped ops (`sincos`, `fma`, `rcp`), common partial CSE, adaptive reordering for register pressure.
- **Memory accessors**: sequential, indexed, shared read/add, blocked struct-of-arrays layouts.
- **GPU solver**: Levenberg–Marquardt + block-Jacobi PCG for factor graphs defined in Python (`@caslib.add_factor`).

Typical robotics uses: bundle adjustment, pose graphs, calibration — **not** tied to the idTech3 game loop.

## Prerequisites

| Requirement | Notes |
|-------------|--------|
| Linux + NVIDIA GPU | Caspar examples use PyTorch CUDA kernels |
| CUDA toolkit | Matches PyTorch / driver (see SymForce `requirements_build.txt`) |
| Python 3.10–3.12 + **python3-venv** | SymForce v0.10.1; Ubuntu: `apt install python3-venv` |
| PyTorch (CUDA) | Required by Caspar examples |

## One-time setup

```bash
./scripts/init_optional_submodules.sh --symforce
./scripts/build_symforce_caspar.sh
./scripts/caspar_check.sh
```

`SYMFORCE_DIR` defaults to `external/symforce` (submodule pinned to tag **v0.10.1**).

## Run upstream examples

```bash
# Custom kernel (ReadShared, AddSharedSum, etc.)
./scripts/run_caspar_kernel_example.sh

# BAL bundle adjustment (needs dataset; see script help)
./scripts/run_caspar_bal_example.sh
```

Generated code lands under SymForce’s `examples/*/generated/` directories.

## Paper reference

Emil Martens, Aaron Miller, Matias Varnum, Annette Stahl — *Caspar: CUDA Accelerator for Symbolic Programming with Adaptive Reordering* ([arXiv:2605.30583](https://arxiv.org/abs/2605.30583)).

The Snavely reprojection factor from Section VI-A is implemented in SymForce at:

`symforce/experimental/caspar/examples/bal/gen_and_run.py`

## Relation to idTech3

- **Default engine build**: unchanged (no SymForce link).
- **Optional**: use Caspar offline for reconstruction, calibration, or tooling that feeds assets into your game base.
- **License**: SymForce is Apache-2.0; respect submodule and PyTorch/CUDA licenses separately.

## Troubleshooting

| Issue | Action |
|-------|--------|
| `No module named symforce.experimental.caspar` | Run `build_symforce_caspar.sh` from source, not `pip install symforce` alone (wheels may omit experimental). |
| CUDA OOM on BAL | Use smaller BAL subsets; Caspar is memory-efficient but large problems need 24GB+ VRAM. |
| No GPU in CI/VM | `caspar_check.sh` skips GPU tests with a warning; static import checks still run. |
