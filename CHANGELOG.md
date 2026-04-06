# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- CTest `test_demo_game_pk3`: verifies `examples/demo_game` zip layout matches CMake packaging (no full `BUILD_EXAMPLE_DEMO_GAME` configure required).
- Vulkan: `vk_procs.c` holds `qvk*` function pointer definitions (split from `vk.c`); `vk_instance.c` no longer duplicates `extern` declarations.
- Vulkan: `vk_shader_modules.c` holds `vk_create_shader_modules` and includes `shader_data.c` / `shader_binding.c` (split from `vk.c`).
- Vulkan: `vk_pipelines_persistent.c` holds `vk_alloc_persistent_pipelines` (split from `vk.c`).
- CHANGELOG.md for release tracking
- Semantic versioning in CMake (project VERSION 1.0.0)
- ENABLE_FORTIFY_SOURCE option for buffer overflow protection (default ON)
- ENABLE_ASAN option for AddressSanitizer in Debug builds
- .editorconfig for consistent editor formatting
- .clang-format for style enforcement (clang-format -i)
- docs/MIGRATION.md for upgrade guidance
- docs/DEPRECATION_POLICY.md for deprecation process
- Unit test infrastructure: tests/unit/, BUILD_UNIT_TESTS option, unit_macros test
- compile_engine.sh asan option for local AddressSanitizer builds
- CI: Clang in Ubuntu build matrix, ASAN job, FORTIFY_SOURCE enabled on Linux

### Changed
- FORTIFY_SOURCE now enabled by default in Release builds (compile_engine.sh)
- Vulkan cinematic path: r_fboCinematic cvar, vk_in_render_pass reset, luminance skip workaround
- Vulkan PBR: direct specular uses **anisotropic GGX** when an anisotropy map is bound (`r_pbr_anisotropicSpecular` default 1); replaces the old roughness-only blend. Re-run `scripts/compile_shaders.sh` after changing `gen_frag.tmpl`.
- Vulkan PBR: **anisotropic visibility** for direct light; optional **IBL roughness stretch** from the anisotropy map (`r_pbr_iblAnisoStretch`); **clearcoat** base attenuation and **Charlie sheen** with optional fourth `sheenScale` roughness token.

### Removed
- Legacy `r_vfog*` engine cvars and `vk_vfog.c`/`vk_vfog.h`: volumetric fog is configured only via `r_volumetricFog*` (and map/`r_fog*` as documented). Editor `worldspawn` keys `vfog_*` remain separate map data, not console cvars.

### Security
- _FORTIFY_SOURCE=2 enabled for GCC/Clang Release builds when ENABLE_FORTIFY_SOURCE=ON

## [1.0.0] - TBD

Initial tagged release. See docs/ROADMAP.md for feature status.

[Unreleased]: https://github.com/.../compare/v1.0.0...HEAD
[1.0.0]: https://github.com/.../releases/tag/v1.0.0
