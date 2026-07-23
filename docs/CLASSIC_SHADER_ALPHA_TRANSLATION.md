# Classic Shader Alpha Translation

How id Tech 3 / OpenArena shader stages map into WBOIT eligibility.

## Supported in certified WBOIT subset

- Texture α
- Texture α × vertex α
- Texture α × constant α (`alphaGen const`)
- Entity opacity (`alphaGen entity` / `rgbGen entity`) when applied once in vertex color

## Routed away from WBOIT

| Case | Path |
|------|------|
| `GLS_ATEST_*` | alpha-tested |
| `ONE / ONE` | additive |
| `ZERO / SRC_COLOR` (modulate) | multiplicative |
| portal / screenMap / refract | refractive (after resolve) |
| `alphaGen portal` | sorted alpha |
| `alphaGen waveform` | sorted alpha |

## Do not infer premul from blend alone

`blendFunc blend` / `SRC_ALPHA ONE_MINUS_SRC_ALPHA` defaults to **straight** (`CLASSIC_COMPATIBILITY`).

## Commands

```text
classic_alpha_translate_status <shader>
material_alpha_status <shader>
```
