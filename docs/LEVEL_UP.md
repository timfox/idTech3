# Level-up integration guide

This document describes the **combined capability stack** on the `cursor/level-up-idtech3-d22f` integration branch and how to validate it locally.

## What is included

| Layer | Feature | Doc / entry point |
|-------|---------|-------------------|
| Renderer | Vulkan Q3/OA compat (HDR lightmap, swapchain, `classic_mod`) | [Q3_OPENARENA_VULKAN.md](Q3_OPENARENA_VULKAN.md) |
| Classic mods | `r_classicMod`, `classic_mod`, OpenArena launcher | [OPENARENA.md](OPENARENA.md) |
| Generative | Spectral-Energy FLUX (`spec_energy_generate`) | [SPEC_ENERGY.md](SPEC_ENERGY.md) |
| QA | Automated beta trace record/replay (`beta_status`, `test_result=`) | [BETA_AUTOMATED_TESTING.md](BETA_AUTOMATED_TESTING.md) |
| CI | Windows-safe binary scans, MSVC qcommon parity, shader determinism | `scripts/q3_openarena_compat_check.sh`, `tests/scripts/test_msvc_qcommon_parity.sh` |

## One-command validation

```bash
./scripts/compile_engine.sh vulkan
./scripts/level_up_validate.sh release
```

Subset for OpenArena only:

```bash
./scripts/openarena_validate.sh release
```

## Playing OpenArena-style QVM mods

```bash
export OA_BASE=/path/to/openarena/base
./scripts/run_openarena.sh
# or conservative Vulkan:
CLASSIC_MOD=1 ./scripts/run_openarena.sh
```

## Recording a gameplay regression trace

```bash
./release/idtech3 +set cl_betaTrace 1
beta_record my_run
# play level ...
beta_mark_success level_complete
beta_stop
beta_test my_run
```

## Merge target

Land on `main` when CI is green across Linux + Windows release matrices. Prefer squashing integration commits with a clear CHANGELOG entry.
