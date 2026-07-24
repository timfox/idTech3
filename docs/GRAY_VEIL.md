# Gray Veil / Elevated Blacks

See also [`SCENE_BRIGHTNESS.md`](SCENE_BRIGHTNESS.md) for global underexposure.

**Commands:** `gray_veil_status`, `gray_veil_bisect`, `gray_veil_capture`,
`renderer_gray_veil_certify`, `auto_exposure_gray_status`

## First-stage finding (outdoor HDR Surf)

With `r_volumetricFog 0` and `r_bloom 0`:

1. **Local exposure shadow lift** (when enabled) — gray veil / elevated blacks.
2. **Filmic white-point predivide** — `x/wp` before the curve mapped middle gray
   `0.18` → ~`0.07` at `wp=2.5` (global dim / crushed midtones). Fixed to
   `FilmicPartial(x)/FilmicPartial(wp)`.

## Corrections

- Local exposure off on HDR sky maps.
- Filmic Hable white-point normalize; AE target `0.18`, min floor `0.70`.
- Keep automatic exposure enabled.

## Bisect

```text
scene_brightness_bisect 1
middle_gray_validate
renderer_brightness_certify
```
