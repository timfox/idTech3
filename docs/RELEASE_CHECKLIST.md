# Release Checklist

Use this checklist before tagging or publishing a release.

For a **stricter engine bar** (automated steps + optional content-backed maps), see [PRODUCTION_CERTIFICATION.md](PRODUCTION_CERTIFICATION.md) and run `./scripts/production_readiness.sh` (set `GAME_BASE` when you have a full `base/` tree). **Gap report:** `./scripts/evidence_status.sh`.

## Pre-Release

### Build Validation
- [ ] **Local CI validation** (optional): `./scripts/validate_ci_build.sh` runs shader compile, Vulkan build, and smoke test
- [ ] **Production readiness orchestrator** (optional): `./scripts/production_readiness.sh` - extends validation with full `ctest` and, if `GAME_BASE` is set, `renderer_regression_maps.sh`
- [ ] **Vulkan build**: `./scripts/compile_engine.sh vulkan` succeeds
- [ ] **Debug build**: `./scripts/compile_engine.sh vulkan debug` succeeds
- [ ] **Shader regeneration**: If shader sources (`.tmpl`, `.vert`, `.frag`, etc.) changed, run `./scripts/compile_shaders.sh --apply` and commit the updated `shader_data.c` and `shader_binding.c`

### Smoke Test
- [ ] **Smoke test passes**: `./scripts/smoke_test.sh release` (run after build; requires `idtech3` and `idtech3_server` in `release/`)

### Renderer regression (headless)
- [ ] **Renderer regression check**: `./scripts/renderer_regression_check.sh` (docs + generated shader blobs + recursive GLSL; optional `GAME_BASE=...` with uncommented paths in `docs/samples/renderer_regression/OPTIONAL_GAME_ASSETS.txt` to require BSPs)
- [ ] **Map load sanity** (when full `base/` + regression pk3 available): `GAME_BASE=/abs/path/to/base ./scripts/renderer_regression_maps.sh`
- [ ] **Graphics changes**: run the manual short list in [docs/RENDERER_CONFIDENCE.md](RENDERER_CONFIDENCE.md) or the [visual regression pack](samples/renderer_regression/README.md) before release

### Unit Tests
- [ ] **Unit tests pass**: `cd build-vk-Release && ctest -R unit -V` (includes `unit_macros`, `unit_qmath`, `unit_surfaceflags`, `unit_qhelpers`, `unit_crc`, `unit_pathutil`, `unit_msg`, `unit_info`, `unit_cm_bounds`, `unit_parse`, `unit_endian`; or run full `ctest`)

### CI
- [ ] **All CI jobs pass**: Push to `main` or open a PR and verify all builds succeed (Windows MSYS, Windows MSVC, Ubuntu x86_64, Ubuntu ARM, macOS, Android)

## Release Artifacts

### Binaries
- [ ] Windows: `idtech3.exe`, `idtech3_server.exe`; **MSYS2/MinGW** artifacts also need bundled MinGW runtime `.dll` (CI runs `scripts/stage_mingw_runtime_dlls.sh`) plus **OpenAL Soft** (`scripts/stage_openal_windows_dlls.ps1` → `OpenAL32.dll` + `soft_oal.dll` next to the `.exe` files on x64). **MSVC** builds static-link the Vulkan renderer (no separate `idtech3_vulkan.dll`); **MSVC x64** CI still runs the OpenAL bundle for drop-in compatibility with OpenAL-linked binaries. **MSVC ARM64** skips that step (no official ARM64 OpenAL Soft bin zip; stock MSVC client uses **WASAPI/DirectSound**, not `snd_backend_openal.c`).
- [ ] Linux: `idtech3`, `idtech3_server`, `idtech3_vulkan.so`
- [ ] macOS: `idtech3`, `idtech3_server`, renderer plugins

### Documentation
- [ ] `CHANGELOG.md` or release notes updated
- [ ] `docs/` reflects current features
- [ ] `docs/QUICKSTART.md` accurate for release download URL

## Post-Release

- [ ] Tag created with version (e.g. `v1.0.0`)
- [ ] GitHub Release created (draft first, then publish)
- [ ] **Publish release** - this triggers the CI build workflow; the `release-attach` job will build binaries for all platforms (Windows, Linux, macOS, Android) and attach them to the release. Allow 15–30 minutes for all platform archives to appear.
- [ ] Verify all platform archives are attached to the release
- [ ] Announcement (if applicable)
