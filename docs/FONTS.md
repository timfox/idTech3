# FonTS (ICCV 2025) — typography pipeline hook

This engine does **not** embed the [FonTS](https://github.com/ArtmeScienceLab/FonTS) DiT training or inference stack. FonTS is a **separate** Python / PyTorch / diffusers project (Flux + typography and style controls). The idTech3 client adds a small **orchestration** layer so you can run your own FonTS environment from the console alongside the existing **cflux2** [`flux_generate`](AGENTS.md) path.

## What you get in-tree

| Piece | Role |
|--------|------|
| `cl_fonts_enable` | Master toggle (default **0**). |
| `cl_fonts_repo` | Absolute path to a local clone of `ArtmeScienceLab/FonTS`. |
| `cl_fonts_python` | Interpreter for `%P` (default `python3`). |
| `cl_fonts_cmd` | **Shell template** expanded and passed to `system()` (blocking). |
| `fonts_pipeline` | Client command: expands `cl_fonts_cmd` and runs it. |

### Template placeholders

| Token | Replaced with (shell-escaped) |
|--------|-------------------------------|
| `%R` | `cl_fonts_repo` |
| `%B` | Engine default base path (`Sys_DefaultBasePath`) |
| `%P` | `cl_fonts_python` |
| `%A` | Everything after `fonts_pipeline` on the console line |
| `%%` | Literal `%` |

**Security:** this is a developer-only hook (`system()` with your template). Do not point `cl_fonts_cmd` at untrusted input.

## Upstream FonTS reality check

The published `flux+SCA-only/infer_flux+SCA-only.py` uses **hard-coded** benchmark paths and a `fake_argv` block in `if __name__ == "__main__"` rather than a full CLI. Expect to add your own thin wrapper script (or patch paths) inside your FonTS clone before calling it from `cl_fonts_cmd`. See the upstream README for environment setup (`conda` / `pip install -r requirements.txt`) and HuggingFace assets.

References:

- Repository: [https://github.com/ArtmeScienceLab/FonTS](https://github.com/ArtmeScienceLab/FonTS)  
- Paper: *FonTS: Text Rendering with Typography and Style Controls* (ICCV 2025).

## Example workflow

1. Clone FonTS and install its Python environment per upstream README.  
2. Add a small wrapper script **inside your clone** (or on `PATH`) that accepts real CLI arguments and calls `XFluxPipeline` the way you need.  
3. In the client:

```text
set cl_fonts_enable 1
set cl_fonts_repo "/abs/path/to/FonTS"
set cl_fonts_python "python3"
set cl_fonts_cmd "cd \"%R/flux+SCA-only\" && %P /abs/path/to/your_fonTS_wrapper.py %A"
fonts_pipeline --bench /path/bench.json --out /path/out
```

4. Use `flux_generate` / `flux_view` as today for **in-engine** cflux2 images; use `fonts_pipeline` only when you intend to run the **external** FonTS stack.

## Optional smoke script

`scripts/fonts_fonTS_check.sh` only checks that `cl_fonts_repo`-style paths contain expected top-level directories; it does not run inference.
