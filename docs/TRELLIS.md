# Microsoft TRELLIS.2 — runtime image-to-3D (FLUX-style)

This engine does **not** embed the [TRELLIS.2](https://github.com/microsoft/TRELLIS.2) PyTorch/CUDA stack. Like **FLUX** (`flux_generate`), TRELLIS runs as a **runtime asset pipeline**: background jobs, status/cancel, and automatic registration of the produced asset in the renderer.

## Parity with FLUX

| FLUX (2D textures) | TRELLIS (3D models) |
|--------------------|---------------------|
| `flux_generate` | `trellis_generate` |
| `flux_status` | `trellis_status` |
| `flux_cancel` | `trellis_cancel` |
| `flux_view` / `flux_show` | `trellis_view` / `trellis_show` |
| `cl_flux_async` | `cl_trellis_async` (default **1**) |
| Hot-reload texture | `cl_trellis_auto_import` → `RegisterModel` on `.glb` |
| — | `trellis_from_prompt` → FLUX image then TRELLIS mesh |

## Cvars

| Cvar | Default | Role |
|------|---------|------|
| `cl_trellis_enable` | 0 | Master toggle |
| `cl_trellis_async` | 1 | Background SDL thread (recommended) |
| `cl_trellis_auto_import` | 1 | Register GLB when job completes |
| `cl_trellis_chain` | 0 | After async FLUX, auto-start TRELLIS on PNG output |
| `cl_trellis_repo` | — | Path to TRELLIS.2 git checkout |
| `cl_trellis_conda` | trellis2 | Conda env for default command |
| `cl_trellis_hf_model` | microsoft/TRELLIS.2-4B | Hugging Face model id |
| `cl_trellis_decimation` | 500000 | GLB decimation target |
| `cl_trellis_texture_size` | 2048 | GLB texture atlas size |
| `cl_trellis_timeout` | 3600 | Warn if job runs longer (seconds) |
| `cl_trellis_cmd` | — | Optional shell template override |

## Runtime workflow

### Image → 3D (async, like FLUX)

```text
set cl_trellis_enable 1
set cl_trellis_repo "/abs/path/to/TRELLIS.2"
trellis_generate screenshots/reference.png
trellis_status
// … when complete, model is under models/trellis/ and auto-imported if cl_trellis_auto_import 1
trellis_view
```

### Text → 3D (FLUX + TRELLIS chain)

Requires `USE_FLUX` build and both toggles on:

```text
set cl_flux_enable 1
set cl_trellis_enable 1
set cl_trellis_chain 1
trellis_from_prompt "a carved stone idol with moss"
```

This runs `flux_generate` in the background, then starts TRELLIS on the FLUX PNG when it finishes.

### Manual FLUX then TRELLIS

```text
set cl_trellis_chain 1
flux_generate "ornate helmet"
// wait for FLUX to complete
trellis_status
```

## Upstream requirements

- Linux, NVIDIA GPU **≥ 24 GB** VRAM (per Microsoft)
- TRELLIS.2 install: `. ./setup.sh --new-env --basic --flash-attn …`
- Weights: [microsoft/TRELLIS.2-4B](https://huggingface.co/microsoft/TRELLIS.2-4B)

Wrapper script: `release/trellis_image_to_glb.py` (copied on build).

## Commands reference

- `trellis_generate <image> [out.glb]` — start job (sync if `cl_trellis_async 0`)
- `trellis_status` / `trellis_cancel`
- `trellis_view [glb]` — register model (default: last job output)
- `trellis_show <glb>` — same as import/register
- `trellis_import <glb>` — alias of show
- `trellis_pipeline` — raw `cl_trellis_cmd` template (advanced)
- `trellis_from_prompt <text>` — FLUX → TRELLIS chain

## Build

`USE_TRELLIS` (default ON). Disable with `-DUSE_TRELLIS=OFF`.

Optional check (no GPU): `./scripts/trellis_check.sh /path/to/TRELLIS.2`
