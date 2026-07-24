# Ghost / fullbright split diagnosis (Surf aztec screenshot class)

## Findings (2026-07-23)

| Tag | Result |
|-----|--------|
| `FIRST_STAGE_LOSING_LIGHTING` | BSP30 bridge wrote **solid white** vertex colors (`LIGHTMAP_BY_VERTEX`) and ignored `BSP30_LUMP_LIGHTING` (~951 KiB on `surf_aztec`). |
| `FULLBRIGHT_ROOT_CAUSE` | `UNLIT_ALBEDO_COMPOSITE` / white-vertex escape. Weapon still shaded via MD3/PBR path. |
| `EXPOSURE_CLIPPING` (amplifier) | `SkyboxHDR_EnableEyeAdaptation` forced `r_exposure_auto 1` with `r_exposureSkyWeight 0.85`, flattening world contrast toward display white against HDR sky. |
| `FIRST_PASS_INTRODUCING_NEGATIVE_GHOST` (primary suspect) | `r_temporalSSR` (default 1) with `r_ssr 1` — stale SSR history / edge contamination. Secondary: AV half-res upsample, bloom. |
| `GHOST_HISTORY_OWNER` | SSR temporal history until quarantine proves otherwise (`r_historyQuarantine 4` or `8`). |

## Fixes shipped

1. Sample GoldSrc luxels (style 0) into BSP30 vertex colors.
2. Stop forcing eye adaptation on HDR sky maps; opt-in via `r_skyboxHDR_autoExposure`.
3. Surf: `r_temporalSSR 0`; aztec mapscript: fixed `r_exposure`.
4. Commands: `ghost_lighting_*`, `fullbright_*`, `history_quarantine_apply`.
5. Reference profile: `exec config/ghost_fullbright_ref.cfg`.

## Verify

```text
map surf_aztec
# load log should include: ...BSP30 lighting: N faces sampled...
exec config/ghost_fullbright_ref.cfg
ghost_lighting_status
fullbright_status
```

Regression: `tests/scripts/test_bsp30_lightmap_vertex.sh`, `tests/scripts/test_ghost_fullbright_regression.sh`.
