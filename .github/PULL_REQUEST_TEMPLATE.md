## Summary

<!-- What changed and why (s&box-style: scope + motivation). -->

## Test plan

- [ ] `./scripts/bootstrap.sh engine`
- [ ] `./scripts/smoke_test.sh ./build-vk-Release` (or relevant subset)
- [ ] Manual: client/server with mod data if applicable
- [ ] **Manifest updated** if sources moved or CMake gates changed (`docs/ENGINE_MODULE_MANIFEST.md`)
- [ ] `./scripts/ci/audit_unconditional_sources.sh` (when touching CMake source lists)
