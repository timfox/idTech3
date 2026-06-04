# Tests

## ctest presets (unified)

From the build directory after configuring with tests enabled:

```bash
ctest --output-on-failure                    # all registered tests
ctest -L unit                              # unit_* binaries
ctest -L validation                          # smoke, renderer_regression, gpu_golden, scripts
ctest -R gpu_golden                          # Tier A golden manifest (no GPU)
ctest -R renderer_regression_check           # 125+ source/shader guards
```

CI runs the same labels via `.github/workflows/build.yml` (`make test` / `ctest`).

- **Unit** (`BUILD_UNIT_TESTS=ON`): `unit_macros`, `unit_qmath`, `unit_surfaceflags`, `unit_qhelpers`, `unit_crc`, `unit_pathutil`, `unit_msg`, `unit_info`, `unit_cm_bounds`, `unit_parse`, `unit_endian` (CRC, COM path, `Info_*`, `COM_Parse*`, and endian tests use the same minimal `stub_qcommon_min.c` + `q_shared.c` + `q_math.c` link as `unit_qhelpers`; `unit_msg` links `msg.c` + `huffman_static.c` with `stub_qcommon_min.c`, `stub_msg_cvar.c`, and `-DDEDICATED`; `unit_cm_bounds` links `cm_bounds.c` + `q_math.c` only) — run `ctest -R unit_` or `./unit_*` from the build directory.
- **Script regression tests**: `test_botlib_bounded_strings` (botlib string invariants). Run with `ctest -R test_botlib_bounded_strings` from the build directory.
- **GPU golden (Tier A)**: `gpu_golden_compare` — `./scripts/gpu_golden_capture.sh --compare` (manifest + placeholder; Tier B: [docs/GPU_GOLDEN_TIER_B.md](../docs/GPU_GOLDEN_TIER_B.md)).
- **i18n / assets / crash hooks**: `test_check_loc`, `test_validate_assets`, `test_crash_report`.
- **Validation**: `smoke_test`, `renderer_regression_check`, `gpu_golden_compare`, `check_artifacts`, `test_run_vulkan_script`, `test_compile_engine_lto`, `test_demo_game_pk3`, `test_vk_vegetation_dispatch_order`, `test_vulkan_mesh_shader_opt_in`, `test_vulkan_runtime_regressions`, `test_botlib_chat_message_bounds`, `test_vulkan_renderer_guards`, `test_vulkan_regression_source_guards`, `test_gltf_vulkan_regressions` (legacy test name may still be `test_gltf_opengl_regressions` in CMake), `test_botlib_bounded_strings` — see `scripts/`, `tests/scripts/`, and `docs/RENDERER_CONFIDENCE.md`.
