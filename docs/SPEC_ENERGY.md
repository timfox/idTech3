# Spectral-Energy Guided Attention — hi-res FLUX generation

This engine does **not** embed the PyTorch/diffusers stack from [rajabi2001/sega](https://github.com/rajabi2001/sega) (research code for **Spectral-Energy Guided Attention**). Like **FLUX** (`flux_generate`) and **TRELLIS** (`trellis_generate`), this feature runs as a **runtime asset pipeline**: background jobs, status/cancel, and optional texture hot-reload when generation completes.

The method dynamically rescales attention across RoPE components from latent spatial-frequency content at each denoising step, improving high-resolution synthesis without retraining. This integration targets the upstream **FLUX** path (`flux_sega/`); Qwen-Image support lives in upstream `qwen_sega/` and can be wired similarly if needed.

> **Naming:** Engine commands and cvars use `spec_energy_*` to avoid confusion with the game company SEGA.

## Parity with FLUX / TRELLIS

| FLUX (fast 2D) | Spec-energy (hi-res 2D) | TRELLIS (3D) |
|----------------|-------------------------|--------------|
| `flux_generate` | `spec_energy_generate` | `trellis_generate` |
| `flux_status` | `spec_energy_status` | `trellis_status` |
| `flux_cancel` | `spec_energy_cancel` | `trellis_cancel` |
| `flux_view` | `spec_energy_view` | `trellis_view` |
| `cl_flux_async` | `cl_spec_energy_async` (default **1**) | `cl_trellis_async` |

## Cvars

| Cvar | Default | Role |
|------|---------|------|
| `cl_spec_energy_enable` | 0 | Master toggle |
| `cl_spec_energy_async` | 1 | Background SDL thread (recommended) |
| `cl_spec_energy_auto_view` | 0 | Hot-reload texture when job completes |
| `cl_spec_energy_repo` | — | Path to upstream git checkout (vendored: `external/flux_spec_energy`) |
| `cl_spec_energy_conda` | spec_energy | Conda env for default command |
| `cl_spec_energy_python` | python3 | Python interpreter (`%P`) |
| `cl_spec_energy_width` / `cl_spec_energy_height` | 4096 | Output resolution |
| `cl_spec_energy_steps` | 28 | Denoising steps |
| `cl_spec_energy_seed` | 0 | Random seed |
| `cl_spec_energy_checkpoint` | Krea-dev | FLUX checkpoint shorthand or HF repo id |
| `cl_spec_energy_multi_gpu` | 0 | Pass `--multi-gpu` when 2+ CUDA devices visible |
| `cl_spec_energy_timeout` | 7200 | Warn if job runs longer (seconds) |
| `cl_spec_energy_cmd` | — | Optional shell template override |

## Runtime workflow

```text
set cl_spec_energy_enable 1
set cl_spec_energy_repo "/abs/path/to/flux_spec_energy"
spec_energy_generate "a misty mountain at dawn, ultra detailed"
spec_energy_status
// … when complete:
spec_energy_view
```

Outputs land under `screenshots/spec_energy/spec_energy_<timestamp>.png` relative to the engine base path.

## Upstream requirements

- Linux, NVIDIA GPU with sufficient VRAM (4096² often needs CPU offload or multi-GPU; see upstream README)
- Python env with PyTorch + diffusers per `external/flux_spec_energy/requirements.txt`
- FLUX weights fetched from Hugging Face on first run

Wrapper script: `release/spec_energy_flux_generate.py` (copied on build).

## Commands

- `spec_energy_generate <prompt>` — start job (sync if `cl_spec_energy_async 0`)
- `spec_energy_status` / `spec_energy_cancel`
- `spec_energy_view [png]` — reload texture and register shader (default: last job output)

## Build

`USE_SPEC_ENERGY` (default ON). Disable with `-DUSE_SPEC_ENERGY=OFF`.

Optional checks (no GPU):

- `./scripts/spec_energy_runtime_check.sh release` — client symbols + wrapper script
- `./scripts/spec_energy_check.sh /path/to/upstream` — upstream repo layout
- `python3 -m py_compile release/spec_energy_flux_generate.py`

Background jobs finalize on the **main thread**; use `spec_energy_view` after completion for hot-reload.

## References

- Paper: [arXiv:2605.22668](https://arxiv.org/abs/2605.22668)
- Upstream research repo: [github.com/rajabi2001/sega](https://github.com/rajabi2001/sega)
