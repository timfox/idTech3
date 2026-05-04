# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Filesystem: `FS_LoadLibrary` extracts native `.so`/`.dll` modules from `.pk3` into `vm/native_cache/` under the game home path (CRC-checked) before `dlopen`, when **`com_nativeLibraryExtractPk3`** is **1** (default; startup log line).
- Lua: `script_reload` falls back to `FS_ReadFile` + `luaL_loadbuffer` when a script lives only inside a pack (virtual paths).
- `examples/demo_game`: `demo_lua.cfg`, `scripts/lua/demo_hooks.lua`, and `exec demo_lua.cfg` from `autoexec.cfg` (Lua demo when `USE_LUA=ON`); pk3 packs **both** `vm/ui<arch>.so` and `vm/ui.<arch>.so` aliases for native UI probe order.
- Vulkan Forward+: **`r_forwardPlusLuminanceSort`** (0/1, archived, default **1**); push constant + **`forward_plus_tile_cull.comp`** partial selection by RGB sum when a tile exceeds **`r_forwardPlusMaxPerTile`**; **`compile_shaders.sh --apply`** updates SPIR-V blobs.
- docs/CURL_NETWORKING.md: tutorial for client libcurl usage (build, `cl_dlURL` / `sv_dlURL`, `download`/`dlmap`, `DLF_*` flags, security, extending for MOTD/API/music).
- `examples/demo_game`: `idtech3_demo.pk3` embeds a minimal native UI module (`vm/ui<arch>.so` or `.dll`) so the demo skeleton can open a window without retail `ui.qvm` (`examples/demo_game/native/ui_skeleton_stub.c`, CMake target `demo_ui_skeleton`).
- `examples/demo_skeleton/`: user-friendly demo playfield (`./scripts/run_demo.sh`, auto-detect layout, `baseq3` hint, help text); `scripts/run_demo.sh` entry point; `base/` + `idtech3_demo/` README stubs.
- CTest `test_demo_game_pk3`: verifies `examples/demo_game` zip layout (configs + optional **`cc`**-built **`vm/ui*.so`**) matches CMake staging.
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
- Docs: [ARCHITECTURE.md](docs/ARCHITECTURE.md) and [QUICKSTART.md](docs/QUICKSTART.md) describe pk3 native library extraction (`vm/native_cache/`, **`com_nativeLibraryExtractPk3`**).
- Client: clearer message when UI VM fails to load (idtech3_demo ships native UI in `vm/`, not configs-only).
- Vulkan Forward+: `vk_forward_plus_dispatch_tile_cull` passes **`luminanceSort`** in push constants; init logs when sort is on.
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
