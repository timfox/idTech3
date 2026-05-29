# Renderer validation - findings (rolling log)

Append new sessions here or add dated files alongside this README. Use [TEMPLATE_TIER_C.md](TEMPLATE_TIER_C.md) for **Tier C** (manual GPU) rows.

## Tier C - manual GPU / validation layers

| Date | Commit | Vulkan | OpenGL | Validation layers | Notes |
|------|--------|--------|--------|-------------------|-------|
| - | - | - | - | - | *Add a row when you complete a real GPU pass (see template). Not a substitute for Tier B (`GAME_BASE` regression on a runner).* |

## Tier A - automated baseline (reference only)

Machine-verified checks that run without a full `GAME_BASE` or display server. **These rows are not Tier C evidence.**

| Date | Commit | Checks | Notes |
|------|--------|--------|-------|
| 2026-04-06 | *see git log for this file* | `ctest` (unit + smoke + demo pk3 layout), `smoke_test.sh`, `renderer_regression_check.sh` (no `GAME_BASE`) | Linux x86_64, Vulkan Release build dir. Confirms scripts + GLSL validation + dedicated smoke; **no** client framebuffer proof. SHA: `git log -1 --format=%H -- docs/renderer_validation/FINDINGS.md`. |
| 2026-05-29 | `4b456bd2` (branch) | `ctest` 30 tests incl. `q3_openarena_compat_check`, `renderer_regression_maps_devdata`; `run_renderer_tier_b_devdata.sh` (6 stub BSP loads) | Headless Linux VM. Q3/OA static compat + Tier B devdata; **no** retail pk3 or client GPU proof. |

When this table grows, consider one file per release (e.g. `FINDINGS_v1.0.2.md`).
