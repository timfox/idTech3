## Summary

<!-- What changed and why (s&box-style: scope + motivation). -->

**Branch:** must target `main` from `feature/*`, `fix/*`, `docs/*`, or `chore/*` (see [docs/BRANCHES.md](docs/BRANCHES.md)). No new long-lived integration branches.

## Test plan

- [ ] `./scripts/bootstrap.sh engine`
- [ ] `./scripts/smoke_test.sh ./build-vk-Release` (or relevant subset)
- [ ] Manual: client/server with mod data if applicable
- [ ] **Manifest updated** if sources moved or CMake gates changed (`docs/ENGINE_MODULE_MANIFEST.md`)
- [ ] `./scripts/ci/audit_unconditional_sources.sh` (when touching CMake source lists)
