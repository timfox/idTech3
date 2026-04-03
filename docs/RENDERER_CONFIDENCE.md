# Renderer confidence checklist

This document lists **automated** checks you can run today and **manual** passes that still require a GPU, game data, or artist judgment. Use it to tighten the Vulkan core and OpenGL fallback without turning every idea into a CI job.

## Automated (headless / CI-friendly)

Run these after any renderer, shader, or mikktspace change.

| Check | Command | What it proves |
|--------|---------|----------------|
| Full local CI parity | `./scripts/validate_ci_build.sh` | SPIR-V generation, Vulkan Release build, release smoke (binaries, server boot, GLSL validation). |
| CMake smoke + artifacts | `ctest --output-on-failure` (from `build-vk-Release` or `build-gl-Release`) | Same smoke as above plus artifact/unit hooks wired into the build. |
| OpenGL matrix | `./scripts/compile_engine.sh opengl` then `ctest` in `build-gl-Release` | Fallback renderer still links and tests pass. |
| Standalone GLSL | `./scripts/smoke_test.sh` (or smoke test phase inside `validate_ci_build.sh`) | Every `.vert`, `.frag`, `.geom`, and `.comp` under `src/renderers/vulkan/shaders/glsl/` validates with `glslangValidator` (recursive, including `volumetric/`, `terrain/`, `postfx/`). |

Optional: `SKIP_IDPAK_CHECK=ON` is normal for engine-only trees; the dedicated server may exit with “no game data” after init — that is still a useful crash-free signal.

## Manual (GPU + content)

These do not run in headless CI; keep a short personal or team list of maps/scenes and settings.

1. **Tangent / normal maps** — Load meshes that rely on MikkTSpace tangents; inspect lighting seams at UV splits and mirrored UVs. Regress after changes to `mikktspace` or normal-map sampling.
2. **PBR materials** — Scenes with metalness/roughness, clearcoat, and normal maps; toggle `r_pbr_debug` modes where applicable. See [PBR_TEXTURES.md](PBR_TEXTURES.md).
3. **Vulkan vs OpenGL** — Same map, same cvars: confirm no silent fallback-only bugs (water, fog, postFX).
4. **Validation** — Debug build or `r_vulkan_validation` (see project cvars/docs): clean validation for a representative play session.
5. **MSAA / SMAA / SSAO** — Spot-check toggles; watch for black screens, NaN tint, or resolution mismatches.

## Repo discipline

- Prefer **first-party** correctness and warning fixes; touch **vendored** code only when the fix is small and behavior-preserving (e.g. const correctness).
- Keep **README** and this doc aligned: what is shipping vs in progress lives in [RENDERERS_FUTURE.md](RENDERERS_FUTURE.md) and [RENDERERS.md](RENDERERS.md).

## See also

- [RENDERERS.md](RENDERERS.md) — feature inventory and cvars
- [RELEASE_CHECKLIST.md](RELEASE_CHECKLIST.md) — pre-release steps
- `./scripts/validate_ci_build.sh` — local mirror of Ubuntu CI expectations
