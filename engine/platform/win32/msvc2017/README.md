# MSVC 2017 projects (Windows)

Hand-maintained Visual Studio solution for **Windows MSVC** builds. Linux/macOS use **CMake + Ninja** (`./scripts/compile_engine.sh vulkan`).

## Active projects

| vcxproj | Output |
|---------|--------|
| `quake3e.vcxproj` | Client (`idtech3.exe`) — links `vulkan.lib` |
| `quake3e-ded.vcxproj` | Dedicated server (`idtech3_server.exe`) |
| `botlib.vcxproj` | Bot library |
| `vulkan.vcxproj` | Vulkan renderer static lib |
| `libjpeg` / `libogg` / `libvorbis` | Third-party deps |

## Deprecated / unmaintained

| vcxproj | Status |
|---------|--------|
| `renderer2.vcxproj` | **Deprecated** — removed from `quake3e.sln`; OpenGL tree deleted. Not built in CI. Use `vulkan.vcxproj`. |
| `opengl.vcxproj` | **Removed** — legacy OpenGL plugin |

## Phase 5d manifest sync

CMake exports `msvc_source_manifest.json`; sync tools keep `ClCompile` lists aligned:

```bash
./scripts/msvc/sync_all_vcxproj.sh
ctest -R test_msvc_manifest_drift
```

See [docs/MSVC_CODEGEN.md](../../../../docs/MSVC_CODEGEN.md) and `IdTech3Layout2026.props`.

**Layout:** sources live under `engine/`, `runtime/`, `modules/`, `renderers/`, `third_party/`. Bridge symlinks under `engine/platform/` resolve `..\..\client\` paths from this directory.
