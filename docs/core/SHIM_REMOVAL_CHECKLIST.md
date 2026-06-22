# Phase 5e: `src/*` shim removal checklist

**Prerequisite:** Phase 5c (physical move) and Phase 5d (MSVC manifest sync) complete.

## Before removing shims

1. Run `./scripts/audit_src_shim_references.sh` — review CMake `src/*` vs `engine/` / `runtime/` / `modules/` counts.
2. Migrate **CMake manifests** (`cmake/**/*.cmake`, `CMakeLists.txt`) to canonical `IDTECH3_DIR_*` paths only.
3. Update **test scripts** that hardcode `src/client/...` to accept both shim and canonical paths (or canonical only).
4. Confirm **MSVC** uses `engine/platform/win32/msvc2017/` (not `src/platform/`) in CI and docs.
5. `ctest -R 'test_repository_layout_2026|test_legacy_intact|test_msvc_'` — all green.
6. Two-week soak on `main` per [DEPRECATION_POLICY.md](../DEPRECATION_POLICY.md).

## Removal steps

```bash
# After manifests/tests migrated:
./scripts/migrate_phase_5e_drop_shims.sh   # (future) remove src/* symlinks + layout bridge symlinks
```

## Keep until mod ecosystem catches up

- QVM / pk3 mod paths are unchanged (game data, not engine `src/` tree).
- Downstream forks may still document `src/qcommon`; point them at `engine/core/`.

See [REPOSITORY_LAYOUT_2026.md](REPOSITORY_LAYOUT_2026.md), [ENGINE_REORG_PLAN.md](../ENGINE_REORG_PLAN.md).
