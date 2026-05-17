# Microsoft TRELLIS.2 — image-to-3D runtime hook

This engine does **not** embed the [TRELLIS.2](https://github.com/microsoft/TRELLIS.2) PyTorch/CUDA stack (4B-parameter image-to-3D, O-Voxel, PBR GLB export). TRELLIS.2 is a **separate** Linux + NVIDIA GPU project. The idTech3 client adds an **orchestration** layer so you can run inference from the console and import `.glb` assets through the existing glTF loader.

## What you get in-tree

| Piece | Role |
|--------|------|
| `cl_trellis_enable` | Master toggle (default **0**). |
| `cl_trellis_repo` | Absolute path to a local clone of `microsoft/TRELLIS.2`. |
| `cl_trellis_conda` | Conda env name for the default command (default `trellis2`). |
| `cl_trellis_python` | Python interpreter token `%P` when not using conda. |
| `cl_trellis_hf_model` | Hugging Face id (default `microsoft/TRELLIS.2-4B`). |
| `cl_trellis_decimation` / `cl_trellis_texture_size` | GLB export knobs (`%D`, `%T`). |
| `cl_trellis_cmd` | Optional shell template (blocking `system()`). |
| `trellis_generate` | Image → GLB via bundled `trellis_image_to_glb.py`. |
| `trellis_pipeline` | Run `cl_trellis_cmd` with template expansion. |
| `trellis_import` | `RegisterModel` on a VFS path (e.g. `models/trellis/foo.glb`). |
| `release/trellis_image_to_glb.py` | Copied next to `idtech3` by `compile_engine.sh`. |

### Template placeholders

| Token | Replaced with (shell-escaped) |
|--------|-------------------------------|
| `%R` | `cl_trellis_repo` |
| `%B` | Engine default base path (`Sys_DefaultBasePath`) |
| `%E` | Same as `%B` (release / install dir with the wrapper script) |
| `%P` | `cl_trellis_python` |
| `%N` | `cl_trellis_conda` |
| `%I` | Input image path (`trellis_generate` only) |
| `%O` | Output GLB path (`trellis_generate` only) |
| `%M` | `cl_trellis_hf_model` |
| `%D` | `cl_trellis_decimation` |
| `%T` | `cl_trellis_texture_size` |
| `%A` | Extra console arguments |
| `%%` | Literal `%` |

**Security:** developer-only hook (`system()` with your template). Do not point `cl_trellis_cmd` at untrusted input.

## Upstream requirements

Per [TRELLIS.2 README](https://github.com/microsoft/TRELLIS.2):

- Linux, NVIDIA GPU with **≥ 24 GB** VRAM (verified on A100/H100 class hardware)
- CUDA toolkit, conda, Python 3.8+
- Install: `. ./setup.sh --new-env --basic --flash-attn --nvdiffrast --nvdiffrec --cumesh --o-voxel --flexgemm`
- Weights: [microsoft/TRELLIS.2-4B](https://huggingface.co/microsoft/TRELLIS.2-4B) (downloaded on first run)

Inference is **blocking** and may take tens of seconds to minutes depending on resolution and GPU.

## Example workflow

1. Clone and install TRELLIS.2 per upstream docs; create conda env `trellis2`.
2. Build the engine (`./scripts/compile_engine.sh vulkan`) so `release/trellis_image_to_glb.py` exists.
3. Place or generate a reference image under your game `base/` tree.
4. In the client:

```text
set cl_trellis_enable 1
set cl_trellis_repo "/abs/path/to/TRELLIS.2"
trellis_generate screenshots/reference.png
trellis_import models/trellis/trellis_12345.glb
```

5. Use the registered model path in maps (`misc_model` or your game's model entity). Vulkan and OpenGL renderers load `.glb` via the glTF path.

### FLUX → TRELLIS chain

You can generate a reference image in-engine with `flux_generate`, then feed it to TRELLIS:

```text
set cl_flux_enable 1
flux_generate "a stylized stone idol"
trellis_generate flux_123456_256x256.png
```

## Custom command template

```text
set cl_trellis_cmd "conda run -n %N --no-capture-output %P \"%E/trellis_image_to_glb.py\" --repo \"%R\" --image \"%I\" --output \"%O\" --model \"%M\" --decimation %D --texture-size %T %A"
trellis_pipeline --help
```

Leave `cl_trellis_cmd` empty to use the built-in default (same as above).

## Optional smoke script

`./scripts/trellis_check.sh /path/to/TRELLIS.2` checks for `setup.sh`, `example.py`, and `trellis2/` — it does not run GPU inference.

## Build flag

CMake option `USE_TRELLIS` (default **ON**) controls client commands and startup logging. Disable with `-DUSE_TRELLIS=OFF` if you do not want the hook compiled in.
