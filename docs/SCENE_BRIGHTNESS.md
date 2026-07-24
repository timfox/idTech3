# Scene Brightness / Underexposure

**Commands:** `scene_brightness_status`, `scene_brightness_bisect`,
`exposure_math_status`, `middle_gray_status`, `middle_gray_validate`,
`renderer_brightness_certify`

## First-stage finding

With bloom/fog off and AE on, global dimness was dominated by:

**TONEMAP_FILMIC_WHITEPOINT_PREDIVIDE** — `FilmicLuminanceCurve` did
`t = x / whitePoint` then normalized as if `whitePoint == 1`. At
`r_grade_whitePoint 2.5`, scene middle gray `0.18` mapped to ~`0.07`
display-linear — severe midtone/black crush that looked like underexposure.
The weapon stayed relatively brighter because authored viewmodel values sit
higher on the same crushed curve.

Exposure EV sign is correct: `sceneColor *= adaptedExposure` (linear).

## Corrections

1. Filmic: `FilmicPartial(x) / FilmicPartial(whitePoint)` (Hable WP normalize).
2. AE: target `0.18`, min floor `0.70`, skyWeight `0.35`, comp `+0.25 EV`.
3. Grade: toe `0.10`, shoulder `0.30`, whitePoint `1.5`, contrast pivot `0.18`.
4. `r_tonemap 5` = neutral_reference (ACES) for diagnosis.

## Bisect

```text
scene_brightness_bisect 1   // fixed exposure *1 + filmic
scene_brightness_bisect 2   // fixed exposure + neutral tonemap
scene_brightness_bisect 3   // AE + filmic
middle_gray_validate
renderer_brightness_certify
```
