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

### Unit Tests
- [ ] **Unit tests pass**: `cd build-vk-Release && ctest -R unit -V` (or run full `ctest`)

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
- [ ] **Publish release** — this triggers the CI build workflow; the `release-attach` job will build binaries for all platforms (Windows, Linux, macOS, Android) and attach them to the release. Allow 15–30 minutes for all platform archives to appear.
- [ ] Verify all platform archives are attached to the release
- [ ] Announcement (if applicable)
