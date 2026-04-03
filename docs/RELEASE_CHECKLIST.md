# Release Checklist

Use this checklist before tagging or publishing a release.

## Pre-Release

### Build Validation
- [ ] **Local CI validation** (optional): `./scripts/validate_ci_build.sh` runs shader compile, Vulkan build, and smoke test
- [ ] **Vulkan build**: `./scripts/compile_engine.sh vulkan` succeeds
- [ ] **OpenGL build**: `./scripts/compile_engine.sh opengl` succeeds
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
- [ ] Windows: `idtech3.exe`, `idtech3_server.exe`, `idtech3_vulkan*.exe`, `idtech3_opengl.so` (or equivalent)
- [ ] Linux: `idtech3`, `idtech3_server`, `idtech3_vulkan.so`, `idtech3_opengl.so`
- [ ] macOS: `idtech3`, `idtech3_server`, renderer plugins

### Documentation
- [ ] `CHANGELOG.md` or release notes updated
- [ ] `docs/` reflects current features
- [ ] `docs/QUICKSTART.md` accurate for release download URL

## Post-Release

- [ ] Tag created with version (e.g. `v1.0.0`)
- [ ] GitHub Release created (draft first, then publish)
- [ ] CI `release-attach` job uploads build artifacts to the release
- [ ] Announcement (if applicable)
