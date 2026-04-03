# Tests

- **Unit** (`BUILD_UNIT_TESTS=ON`): `unit_macros`, `unit_qmath`, `unit_surfaceflags`, `unit_qhelpers`, `unit_crc`, `unit_pathutil` (CRC and COM path tests use the same minimal `stub_qcommon_min.c` + `q_shared.c` + `q_math.c` link as `unit_qhelpers`) — run `ctest -R unit_` or `./unit_*` from the build directory.
- **Validation**: `smoke_test`, `renderer_regression_check`, `check_artifacts` — see `scripts/` and `docs/RENDERER_CONFIDENCE.md`.
