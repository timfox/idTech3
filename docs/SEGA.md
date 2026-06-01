# SEGA — hi-res FLUX generation (Spectral-Energy Guided Attention)

This engine does **not** embed the [SEGA](https://github.com/rajabi2001/sega) PyTorch/diffusers stack. Like **FLUX** (`flux_generate`) and **TRELLIS** (`trellis_generate`), SEGA runs as a **runtime asset pipeline**: background jobs, status/cancel, and optional texture hot-reload when generation completes.

SEGA is a training-free method that rescales attention across RoPE components from latent spatial-frequency content at each denoising step, improving high-resolution synthesis without retraining. This integration targets the **FLUX** path (`flux_sega/`); Qwen-Image support lives in upstream `qwen_sega/` and can be wired similarly if needed.

## Parity with FLUX / TRELLIS

| FLUX (fast 2D) | SEGA (hi-res 2D) | TRELLIS (3D) |
|----------------|------------------|--------------|
| `flux_generate` | `sega_generate` | `trellis_generate` |
| `flux_status` | `sega_status` | `trellis_status` |
| `flux_cancel` | `sega_cancel` | `trellis_cancel` |
| `flux_view` | `sega_view` | `trellis_view` |
| `cl_flux_async` | `cl_sega_async` (default **1**) | `cl_trellis_async` |

## Cvars

| Cvar | Default | Role |
|------|---------|------|
| `cl_sega_enable` | 0 | Master toggle |
| `cl_sega_async` | 1 | Background SDL thread (recommended) |
| `cl_sega_auto_view` | 0 | Hot-reload texture when job completes |
| `cl_sega_repo` | — | Path to SEGA git checkout (vendored copy: `external/sega`) |
| `cl_sega_conda` | sega | Conda env for default command |
| `cl_sega_python` | python3 | Python interpreter (`%P`) |
| `cl_sega_width` / `cl_sega_height` | 4096 | Output resolution |
| `cl_sega_steps` | 28 | Denoising steps |
| `cl_sega_seed` | 0 | Random seed |
| `cl_sega_checkpoint` | Krea-dev | FLUX checkpoint shorthand or HF repo id |
| `cl_sega_multi_gpu` | 0 | Pass `--multi-gpu` when 2+ CUDA devices visible |
| `cl_sega_timeout` | 7200 | Warn if job runs longer (seconds) |
| `cl_sega_cmd` | — | Optional shell template override |

## Runtime workflow

```text
set cl_sega_enable 1
set cl_sega_repo "/abs/path/to/sega"    // or repo checkout at external/sega
sega_generate "a misty mountain at dawn, ultra detailed"
sega_status
// … when complete:
sega_view
```

Outputs land under `screenshots/sega/sega_<timestamp>.png` relative to the engine base path.

## Upstream requirements

- Linux, NVIDIA GPU with sufficient VRAM (4096² often needs CPU offload or multi-GPU; see upstream README)
- Python env with PyTorch + diffusers per `external/sega/requirements.txt`
- FLUX weights fetched from Hugging Face on first run

Wrapper script: `release/sega_flux_generate.py` (copied on build).

## Commands

- `sega_generate <prompt>` — start job (sync if `cl_sega_async 0`)
- `sega_status` / `sega_cancel`
- `sega_view [png]` — reload texture and register shader (default: last job output)

## Build

`USE_SEGA` (default ON). Disable with `-DUSE_SEGA=OFF`.

Optional checks (no GPU):

- `./scripts/sega_runtime_check.sh release` — client symbols + wrapper script
- `python3 -m py_compile release/sega_flux_generate.py`

Background jobs finalize on the **main thread**; use `sega_view` after completion for hot-reload.

## References

- Paper: [arXiv:2605.22668](https://arxiv.org/abs/2605.22668)
- Upstream: [github.com/rajabi2001/sega](https://github.com/rajabi2001/sega)
