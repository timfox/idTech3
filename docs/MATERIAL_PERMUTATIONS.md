# Material permutation budget

Hybrid deferred + Forward+ must not explode pipeline variant count.

## Policy

- **Mode 0**: Forward+ only (default)
- **Mode 1**: Deferred G-buffer for static geo + Forward+ dynamics (`r_renderMode`)
- New permutations require entry in this doc and grep guard in CI

## CI

`scripts/count_material_permutations.sh` scans `vk_create_pipeline.c` and shader `#ifdef` blocks.  
`renderer_regression_check.sh` warns when count exceeds threshold (default 512).

## Lock checklist

- [ ] Deferred path documented in [FORWARD_PLUS_PIPELINE_AUDIT.md](FORWARD_PLUS_PIPELINE_AUDIT.md)
- [ ] RTX hybrid does not duplicate full forward permutations
- [ ] `r_pbr` / `r_forwardPlus` toggles have fallbacks
- [ ] Material blend uses specialization constants `material_blend_layers` (2..8) / `material_height_mask` (8 bits) and descriptor set 19 layer arrays (not new `#ifdef` FS variants) — see [MATERIAL_BLEND.md](MATERIAL_BLEND.md)
