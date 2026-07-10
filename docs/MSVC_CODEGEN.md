# MSVC / Phase 5d

Hand-maintained Visual Studio projects live under **`engine/platform/win32/msvc2017/`**. After the Phase 5c physical move, their relative `ClCompile` paths (`..\..\client\…` from `msvc2017/`) resolve via **bridge symlinks** created by **`scripts/layout_forwarding_symlinks.sh`**:

| vcxproj prefix | Bridge |
|----------------|--------|
| `engine/platform/client` | → `runtime/client` |
| `engine/platform/qcommon` | → `engine/core` |
| `engine/platform/external` | → `third_party` |
| `engine/botlib`, `engine/physics`, … | → `modules/*` |

Validation: **`ctest -R test_msvc_layout_bridge`**.

## Source manifest

CMake exports logical source groups at configure time:

```bash
./scripts/generate_msvc_source_manifest.sh
# build-msvc-manifest/msvc_source_manifest.json
# groups: qcommon, server, client, botlib, renderer_vulkan, renderer_common
```

Enable manually: **`-DIDTECH3_EXPORT_MSVC_MANIFEST=ON`**.

## Drift checks

```bash
ctest -R test_msvc_manifest_drift
# overlap coverage per group (qcommon/server/client/botlib)
```

List additions without editing vcxproj:

```bash
PYTHONPATH=scripts/msvc python3 scripts/msvc/sync_vcxproj_sources.py \
  --manifest build-msvc-manifest/msvc_source_manifest.json --project quake3e
```

Opt-in sync relocates stale flat `client/*.c` paths to modular `client/core|media|platform/`, then **appends** missing manifest sources into the `IDTECH3_MSVC_MANIFEST_*` block. It refuses to shrink the ClCompile count on write.

**Do not enable realpath ClCompile dedupe casually.** Layout bridges make shim and canonical paths share a realpath; `--dedupe-realpath` previously deleted legitimate entries. Default sync leaves existing ClCompile rows alone; only pass `--dedupe-realpath` when you intentionally want that behavior and have reviewed the dry-run diff.

```bash
PYTHONPATH=scripts/msvc python3 scripts/msvc/sync_vcxproj_sources.py \
  --manifest build-msvc-manifest/msvc_source_manifest.json \
  --project quake3e --write
```

Path resolution CI: **`ctest -R test_msvc_vcxproj_paths_resolve`** (quake3e, quake3e-ded, botlib, vulkan).

Bulk sync all MSVC projects:

```bash
./scripts/msvc/sync_all_vcxproj.sh
```

Shared MSBuild props: **`engine/platform/win32/msvc2017/IdTech3Layout2026.props`** (imported by `quake3e.vcxproj`).

## Remaining work

- `quake3e.vcxproj` synced: 17 modular path relocations + manifest block (~89 sources).
- `quake3e-ded.vcxproj` synced: server/qcommon manifest additions.
- `vulkan.vcxproj` synced: +25 renderer sources (extensions, vector font, deferred, etc.).
- Drift thresholds: qcommon/server/client/botlib/renderer ≥95% overlap with CMake manifest.
- `renderer2.vcxproj` references removed OpenGL renderer paths — skipped by `test_msvc_layout_bridge` until removed.
- Full vcxproj regeneration from manifest (replace hand-maintained lists).
- Drop `src/*` and bridge symlinks once MSVC projects consume canonical paths only.
