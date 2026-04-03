# Tests

- **Unit** (`BUILD_UNIT_TESTS=ON`): `unit_macros`, `unit_qmath`, `unit_surfaceflags`, `unit_qhelpers` (q_shared path/hash/COM helpers + `tests/stub_qcommon_min.c`) — run `ctest -R unit_` or `./unit_*` from the build directory.
- **Validation**: `smoke_test`, `renderer_regression_check`, `check_artifacts` — see `scripts/` and `docs/RENDERER_CONFIDENCE.md`.
