# Caspar examples (SymForce)

Caspar is **not** compiled into `idtech3`. Use the optional SymForce submodule and scripts in the repo root.

## Setup

```bash
./scripts/init_optional_submodules.sh --symforce
./scripts/build_symforce_caspar.sh
./scripts/caspar_check.sh
```

## Run

| Script | Upstream example |
|--------|------------------|
| `./scripts/run_caspar_kernel_example.sh` | `kernel_example` — symbolic CUDA kernel |
| `./scripts/run_caspar_bal_example.sh` | `bal` — Bundle Adjustment in the Large |

## Paper factor (Snavely reprojection)

The residual from Martens et al. (arXiv:2605.30583) lives in upstream:

`external/symforce/symforce/experimental/caspar/examples/bal/gen_and_run.py`

After `build_symforce_caspar.sh`, edit and regenerate from that file; Caspar emits CUDA kernels and a C++/Python solver automatically.

## Docs

[docs/CASPAR.md](../../docs/CASPAR.md)
