# Phase 5e: `src/*` shim removal checklist

**Prerequisite:** Phase 5c (physical move) and Phase 5d (MSVC manifest sync) complete.

## Phase 5e status

| Item | Status |
|------|--------|
| Drop `src/*` forwarding shims | **Done** — `./scripts/migrate_phase_5e_drop_shims.sh --apply` |
| Keep layout bridges (`runtime/qcommon`, `engine/platform/*`, …) | **Kept** — relative `#include` + MSVC still need them |
| `src/README.md` points at canonical roots | Done |
| Layout / legacy / audit tests updated for post-shim world | Done |

### Prep (completed earlier)

| Item | Status |
|------|--------|
| CMake manifests prefer canonical / `IDTECH3_DIR_*` | Done |
| Wiring tests prefer canonical paths with shim fallback | Done (batches A–D) |
| MSVC sync: realpath ClCompile dedupe off by default | Done |
| `__pycache__/` ignored; shim audit budget 50 | Done |

Verify:

```bash
./scripts/layout_forwarding_symlinks.sh
./scripts/audit_src_shim_references.sh --strict
ctest -R 'test_repository_layout_2026|test_legacy_intact|test_msvc_|test_client_modular|test_cpp20|test_openworld|test_no_aux' --output-on-failure
```

## Remaining (not Phase 5e)

- Rewrite relative `#include "../../qcommon/..."` to absolute-from-root or `IDTECH3_DIR_*` includes, then drop **layout bridges**.
- Two-week soak was waived for this drop after prep + test migration on `main`; watch CI / downstream forks.

## Removal command

```bash
./scripts/migrate_phase_5e_drop_shims.sh --dry-run
./scripts/migrate_phase_5e_drop_shims.sh --apply
```

## Keep for mod ecosystem

- QVM / pk3 mod paths are unchanged (game data, not engine `src/` tree).
- Downstream forks documenting `src/qcommon` should switch to `engine/core/` (see `src/README.md`).

See [REPOSITORY_LAYOUT_2026.md](REPOSITORY_LAYOUT_2026.md), [ENGINE_REORG_PLAN.md](../ENGINE_REORG_PLAN.md).
