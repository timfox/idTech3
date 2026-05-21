# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Optional Git submodule **`tools/tiled`**: [Tiled Map Editor](https://www.mapeditor.org/) (GPL-2.0, upstream `mapeditor/tiled`, pinned tag **v1.9.91**); not linked into the engine — see **`docs/TILED.md`**. Init via **`scripts/init_optional_submodules.sh`** (`--tiled`, `--svo`, `--all`, `--dry-run`). Designer sample: **`examples/tiled/minimal_demo.tmx`**; CTest **`test_init_optional_submodules`**.
- Vulkan VDB: console commands **`vdb_load`**, **`vdb_upload`**, **`vdb_bind_fog`**, **`vdb_list`** for NanoVDB → volumetric fog workflow (refreshes volumetric descriptors after upload).
- Vulkan Forward+: **`r_forwardPlusDepthCull`** (0/1, default **0**); when **1**, tile cull runs after opaque geometry and rejects lights behind the depth buffer at each light’s screen center (reversed-Z).
- Vulkan RTX (USE_VULKAN_RTX): **`r_rtxEntities`** / **`r_rtxEntityCap`** (default **0** / **128**); when **1** with **`r_rtxDemo`**, refEntity model bounds are packed as proxy AABB boxes in a second BLAS and included in the TLAS each frame.
- Vulkan VDB: **`r_vdbFog`** / **`r_vdbFogBlend`** (default **0** / **0.5**); blends bound GPU-uploaded VDB fog density into the volumetric fog compute pass (binding 17).
- Vulkan RTX (USE_VULKAN_RTX): **`compile_shaders.sh` auto-writes `src/renderers/vulkan/vk_rtx_demo_spirv.inc`**; per-frame UBO passes **`r_rtx` 1–3** into **`rtx_demo.rchit`** for distinct hit tints (shadow / reflections / full visualization).
- Vulkan ImGui: **File** (screenshot JPEG silent, toggle console, quit), **Help** (shortcuts popup + About inspector with GPU strings), **Developer** (`r_imgui`, `r_speeds` 0–6, `r_showtris` wireframe) menu items.
- Filesystem: `FS_LoadLibrary` extracts native `.so`/`.dll` modules from `.pk3` into `vm/native_cache/` under the game home path (CRC-checked) before `dlopen`, when **`com_nativeLibraryExtractPk3`** is **1** (default; startup log line).
- Lua: `script_reload` falls back to `FS_ReadFile` + `luaL_loadbuffer` when a script lives only inside a pack (virtual paths).
- `examples/demo_game`: `demo_lua.cfg`, `scripts/lua/demo_hooks.lua`, and `exec demo_lua.cfg` from `autoexec.cfg` (Lua demo when `USE_LUA=ON`); pk3 packs **both** `vm/ui<arch>.so` and `vm/ui.<arch>.so` aliases for native UI probe order.
- Vulkan Forward+: **`r_forwardPlusLuminanceSort`** (0/1, archived, default **1**); push constant + **`forward_plus_tile_cull.comp`** partial selection by RGB sum when a tile exceeds **`r_forwardPlusMaxPerTile`**; **`compile_shaders.sh --apply`** updates SPIR-V blobs.
- docs/CURL_NETWORKING.md: tutorial for client libcurl usage (build, `cl_dlURL` / `sv_dlURL`, `download`/`dlmap`, `DLF_*` flags, security, extending for MOTD/API/music).
- Vulkan ImGui: `r_imgui` cvar (default 1) skips inspector CPU work when 0; startup log line in renderer init; client `toggle_imgui` command and **F11** hardcoded toggle when `USE_IMGUI` + Vulkan client build.
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
- Vulkan ImGui: **File → Quit** runs `quit` (clean exit) instead of a no-op.

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
