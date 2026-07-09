# Tests

## ctest presets (unified)

From the build directory after configuring with tests enabled (`BUILD_UNIT_TESTS=ON` by default):

```bash
make test                                  # all registered CTest targets
ctest --output-on-failure                  # same
ctest -L unit                              # unit_* binaries
ctest -L validation                        # smoke, renderer_regression, gpu_golden, scripts
ctest -L sector_stream                     # open-world sector stream path matrix members
ctest -R sector_stream_matrix -V           # wiring guard + runs full stream path suite
make test-sector-stream                    # same matrix without full `make test`
ctest -R gpu_golden                        # Tier A golden manifest (no GPU)
ctest -R renderer_regression_check           # 125+ source/shader guards
```

CI and local parity: `./scripts/validate_ci_build.sh` runs full `ctest`; `./scripts/production_readiness.sh` runs the same after a Vulkan build.

### Sector stream matrix (`sector_stream` label)

End-to-end coverage for **Phase C sector streaming** (collision merge, nav tiles, MP sync list, residency, graph compute, runtime fidelity):

| Layer | CTest name |
|-------|------------|
| Wiring | `test_openworld`, `test_cm_stream_merge`, `test_nav_bake`, `test_openworld_sync`, `test_openworld_residency`, `test_graph_compute`, `test_proc`, `test_demo_openworld_pk3` |
| Runtime | `test_openworld_runtime`, `test_sector_stream_fidelity` |
| Units | `unit_world_residency`, `unit_sector_graph`, `unit_cluster_graph`, `unit_openworld_nav` (when `USE_RECAST_NAV`) |
| Orchestrator | `sector_stream_matrix` — verifies label registration, then runs all of the above |

Quick commands:

```bash
cd build-vk-Release
ctest -L sector_stream --output-on-failure
# or
make test-sector-stream
```

### Other suites

- **Unit** (`BUILD_UNIT_TESTS=ON`): `unit_macros`, `unit_qmath`, … — `ctest -R '^unit_'`
- **Script regression**: `test_botlib_bounded_strings`, renderer guards, temporal (`test_temporal_motion_policy`), vector font mode 2, etc.
- **GPU golden (Tier A)**: `gpu_golden_compare` — `./scripts/gpu_golden_capture.sh --compare`
- **i18n / assets / crash hooks**: `test_check_loc`, `test_validate_assets`, `test_asset_pipeline`, `test_crash_report`
- **Validation**: `smoke_test`, `renderer_regression_check`, `check_artifacts`, **`test_cpp20_sources`** (C→C++20 world layer revert guard), … — see `docs/RENDERER_CONFIDENCE.md`
