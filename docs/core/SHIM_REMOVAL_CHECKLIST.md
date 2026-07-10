# Phase 5e: `src/*` shim removal checklist

**Prerequisite:** Phase 5c (physical move) and Phase 5d (MSVC manifest sync) complete.

## Prep completed (keep shims)

These items are done on trunk; **`src/*` forwarding shims and layout bridges remain** until Phase 5e proper.

| Item | Status |
|------|--------|
| CMake manifests prefer `engine/` / `runtime/` / `modules/` / `renderers/` (and `IDTECH3_DIR_*`) | Done — see `cmake/EngineQcommonSources.cmake`, `cmake/server/`, `cmake/client/`, `cmake/renderers/`, `CMakeLists.txt` |
| High-traffic tests accept canonical paths with shim fallback | Done — `idtech3_test_paths.sh`, client modular/domains, cpp20, legacy intact, vulkan extensions layout |
| MSVC sync: realpath ClCompile dedupe **off** by default; non-shrink guard | Done — `scripts/msvc/sync_vcxproj_sources.py` (`--dedupe-realpath` opt-in only) |
| `__pycache__/` / `*.pyc` ignored | Done — `.gitignore` |
| Shim audit `--strict` CMake budget | Done — `MAX_SHIM_CMAKE=50` (was 500); prints migration progress % |

Verify anytime:

```bash
./scripts/audit_src_shim_references.sh --strict
./tests/scripts/test_msvc_manifest_drift.sh
ctest -R 'test_repository_layout_2026|test_legacy_intact|test_msvc_|test_client_modular|test_cpp20'
```

## Before removing shims (remaining blockers)

1. Run `./scripts/audit_src_shim_references.sh` — CMake shim lines should stay under the strict budget; tests still have many `src/*` greps (~380+) that resolve via symlink.
2. Finish migrating **remaining test scripts** that hardcode `src/...` only (optional for CI green today; required before drop).
3. Confirm **MSVC** uses `engine/platform/win32/msvc2017/` (not `src/platform/`) in CI and docs — already the case; do **not** run sync with `--dedupe-realpath` casually.
4. `ctest -R 'test_repository_layout_2026|test_legacy_intact|test_msvc_'` — all green.
5. Two-week soak on `main` per [DEPRECATION_POLICY.md](../DEPRECATION_POLICY.md).

## Removal steps

```bash
# After manifests/tests migrated:
./scripts/migrate_phase_5e_drop_shims.sh   # (future) remove src/* symlinks + layout bridge symlinks
```

## Keep until mod ecosystem catches up

- QVM / pk3 mod paths are unchanged (game data, not engine `src/` tree).
- Downstream forks may still document `src/qcommon`; point them at `engine/core/`.

See [REPOSITORY_LAYOUT_2026.md](REPOSITORY_LAYOUT_2026.md), [ENGINE_REORG_PLAN.md](../ENGINE_REORG_PLAN.md).
