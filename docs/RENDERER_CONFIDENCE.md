# Renderer confidence checklist

This document lists **automated** checks you can run today and **manual** passes that still require a GPU, game data, or artist judgment. Use it to tighten the Vulkan core and OpenGL fallback without turning every idea into a CI job.

## Release gate (renderer changes)

Treat this file as a **default gate** for any change that touches rendering, shaders, mesh tangents, or image loaders used by the renderer:

1. Run **automated** rows in the table below (at minimum `renderer_regression_check.sh` + your usual build).
2. For **graphics PRs**, run the **manual** short list or note why it is N/A (e.g. docs-only).
3. Prefer **first-party** warning/correctness fixes over new subsystems until the gate is green.

Visual regression **specs** and suggested map names: [samples/renderer_regression/README.md](samples/renderer_regression/README.md).

## Automated (headless / CI-friendly)

Run these after any renderer, shader, or mikktspace change.

| Check | Command | What it proves |
|--------|---------|----------------|
| Renderer regression (repo) | `./scripts/renderer_regression_check.sh` | Regression docs present; `shader_data.c` / `shader_binding.c` exist; recursive GLSL `glslang` pass. Optional: set `GAME_BASE` and uncomment BSP paths in `OPTIONAL_GAME_ASSETS.txt` to require packaged maps. |
| Map load sanity (content) | `GAME_BASE=/abs/path/to/base ./scripts/renderer_regression_maps.sh` | Dedicated server runs `+map` for each `rtest_*` map; log scanned for `ERROR:`, `couldn't load`, `CM_LoadMap`, crashes. Requires full game base (VM + assets), not the regression pk3 alone. `RELEASE_DIR` optional (default `<repo>/release`). |
| Full local CI parity | `./scripts/validate_ci_build.sh` | SPIR-V generation, Vulkan Release build, smoke test, renderer regression check. |
| CMake smoke + artifacts | `ctest --output-on-failure` (from `build-vk-Release` or `build-gl-Release`) | Smoke, **renderer_regression_check**, artifacts, unit hooks. |
| OpenGL matrix | `./scripts/compile_engine.sh opengl` then `ctest` in `build-gl-Release` | Fallback renderer still links and tests pass. |
| Standalone GLSL | `./scripts/smoke_test.sh` (or the smoke step inside `validate_ci_build.sh`) | Every `.vert`, `.frag`, `.geom`, and `.comp` under `src/renderers/vulkan/shaders/glsl/` validates with `glslangValidator` (recursive, including `volumetric/`, `terrain/`, `postfx/`). |

Optional: `SKIP_IDPAK_CHECK=ON` is normal for engine-only trees; the dedicated server may exit with “no game data” after init — that is still a useful crash-free signal.

## Manual (GPU + content)

These do not run in headless CI; use the **[visual regression pack](samples/renderer_regression/README.md)** (scene docs + optional BSP list) so the same six concerns are hit every time.

1. **Tangent / normal maps** — Load meshes that rely on MikkTSpace tangents; inspect lighting seams at UV splits and mirrored UVs. Regress after changes to `mikktspace` or normal-map sampling.
2. **PBR materials** — Scenes with metalness/roughness, clearcoat, and normal maps; toggle `r_pbr_debug` modes where applicable. See [PBR_TEXTURES.md](PBR_TEXTURES.md).
3. **Vulkan vs OpenGL** — Same map, same cvars: confirm no silent fallback-only bugs (water, fog, postFX).
4. **Validation** — Debug build or `r_vulkan_validation` (see project cvars/docs): clean validation for a representative play session.
5. **MSAA / SMAA / SSAO** — Spot-check toggles; watch for black screens, NaN tint, or resolution mismatches.
6. **Emissive** and **volumetric fog** — See scene docs under `samples/renderer_regression/scenes/`.

## Repo discipline

- Prefer **first-party** correctness and warning fixes; touch **vendored** code only when the fix is small and behavior-preserving (e.g. const correctness).
- Keep **README** and this doc aligned: what is shipping vs in progress lives in [RENDERERS_FUTURE.md](RENDERERS_FUTURE.md) and [RENDERERS.md](RENDERERS.md).

## See also

- [RENDERERS.md](RENDERERS.md) — feature inventory and cvars
- [RELEASE_CHECKLIST.md](RELEASE_CHECKLIST.md) — pre-release steps
- `./scripts/validate_ci_build.sh` — local mirror of Ubuntu CI expectations
