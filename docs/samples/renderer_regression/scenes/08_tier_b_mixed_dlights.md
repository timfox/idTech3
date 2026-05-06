# Tier B: mixed point + linear (spot-style) dynamic lights

**Goal:** Validate that both **classic** dlight projection and **Forward+** experimental PBR shade (`r_forwardPlusShade`) behave plausibly when **point** and **linear** (`dlight_t.linear`) lights coexist in the same view.

## Preconditions

- Self-hosted runner with `GAME_BASE` (see `docs/renderer_validation/SELF_HOSTED_TIER_B.md`).
- Built `idtech3_server` and client/renderer as in CI.
- A map that places at least one **point** dlight and one **linear** dlight in the same playable area (author in Radiant / game tools, or a small test BSP checked into your `base/` tree).

## Optional automated map list

If that map is named e.g. `rtest_mixed_dlights`, add it to Tier B load checks without editing the script:

```bash
MAPS_EXTRA="rtest_mixed_dlights" GAME_BASE=/path/to/base ./scripts/renderer_regression_maps.sh
```

Repository variable **`IDTECH3_MAPS_EXTRA`** (space-separated names) is forwarded by `.github/workflows/renderer-tier-b.yml` when set.

## Procedure

1. Run `GAME_BASE=... ./scripts/renderer_regression_maps.sh` (with or without `MAPS_EXTRA`).
2. In-engine (GPU, Tier C): toggle `r_forwardPlusShade` and compare brightness/shape of each light type vs classic dlight-only frames (`r_forwardPlusShade 0`).
3. Confirm **no double-counting** where surfaces set `tess.dlightBits`: Forward+ shade should **skip** indices present in the mask (`pbrForwardPlus.y`).

## Pass / fail

- **Pass:** Map(s) load with no server errors; spot and point contributions look consistent with expectations; no obvious double-lit hotspots on surfaces already hit by the projector pass.
- **Fail:** Load errors, crashes, or clear double illumination on masked surfaces.

## Notes

- Forward+ tile cull uses a **screen-space sphere** approximation for both types; tight spotlight frusta are not modeled.
- `r_temporalCustomShaderMotion` is unrelated but documented for motion-vector policy (see `tr_init.c` / `vk_view_state.c`).
