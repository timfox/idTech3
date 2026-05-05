# AGENTS.md

## Cursor Cloud specific instructions

### Overview

This is an **idTech3 engine fork** - a C/C++ game engine based on Quake III Arena with Vulkan 1.4 + RTX rendering, PBR, audio codecs (Opus/FLAC/WebM/MP3), Lua/Duktape scripting, and ImGui debug UI. It produces a client (`idtech3`), dedicated server (`idtech3_server`), and renderer plugins (`idtech3_opengl.so`, `idtech3_vulkan.so`).

### Building

See `CLAUDE.md` for canonical build commands. The primary build script is `./scripts/compile_engine.sh`. Key examples:

```
./scripts/compile_engine.sh vulkan          # Vulkan renderer, Release
./scripts/compile_engine.sh opengl          # OpenGL renderer, Release
./scripts/compile_engine.sh vulkan debug    # Vulkan renderer, Debug
./scripts/compile_engine.sh vulkan demo     # Also builds idtech3_demo.pk3 → release/demo_game/
./scripts/compile_engine.sh clean vulkan    # Clean build
```

Build artifacts go to `build-vk-Release/` or `build-gl-Release/` and are copied to `release/`.

### Gotchas

- **C++ linker dependency**: The build requires `libstdc++-14-dev` because Clang 18 (the default `c++` on Ubuntu 24.04) selects the GCC 14 installation but only GCC 13 dev files are installed by default. The update script installs this.
- **No game data**: The engine repo does not include Quake III Arena game data (`.pk3` files). The dedicated server will print "No game data" and exit cleanly - this is expected. `SKIP_IDPAK_CHECK=ON` is set by default in `compile_engine.sh`.
- **Test suite**: Run `make test` or `ctest` from the build directory to execute the smoke test (binary checks, server startup, shader validation). Full validation is via build matrix (`.github/workflows/build.yml`) and manual testing.
- **Headless environment**: The client executable (`idtech3`) requires a display server (X11/SDL2) and GPU. In headless Cloud Agent VMs, only the dedicated server (`idtech3_server`) can run. The client binary can still be verified via `file` and `ldd` checks.
- **Shader compilation**: Vulkan GLSL shaders are compiled to SPIR-V during the CMake build via `scripts/compile_shaders.sh`. This requires `glslangValidator` (from `glslang-tools`) and Python 3.

### Linting / Static Analysis

`scripts/run_clang_tidy.sh` and `scripts/run_cppcheck.sh` are available for optional local static analysis. CI primarily enforces quality through compiler warnings (`-Wall -Wextra -Wpedantic` and more), with `CI_BUILD=OFF` in the current workflow so warnings are not treated as errors by default.

### Running

- **Dedicated server**: `./release/idtech3_server +set dedicated 1 +set com_hunkMegs 64`
- **Client** (requires display): `./release/idtech3`
- Both require game data in a `base/` directory to do anything meaningful.

### Game data / base

- **Standalone full conversion**: Do not assume Q3A, OpenArena, or other generic bases. The base is either Unwaking or a game explicitly defined by the user.
- **Smallest valid data tree** (bootstrap `.pk3` + `default.cfg`): see `docs/MINIMAL_GAME_SHELL.md`.
