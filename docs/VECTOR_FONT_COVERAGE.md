# Vector Font Coverage

Coverage is pixel alpha from analytical outline evaluation — not color.

## Modes (`r_vectorFontCoverage`)

| Mode | Behavior |
|------|----------|
| 0 | Dual-axis sample at pixel center (diagnostic) |
| 1 | Dual-axis analytical (production minimum) |
| 2 | Adaptive boundary supersampling (default) |
| 3 | Ultra boundary SS (8 taps) |

Boundary detection: `coverage ∈ (0.02, 0.98)`. Interior/exterior use one sample.

Sample offsets are **stable in pixel space** (no temporal jitter).

## Premultiplied linear blend

```glsl
rgb = color.rgb * (color.a * coverage);
a   = color.a * coverage;
// Blend: ONE, ONE_MINUS_SRC_ALPHA
```

Do not blend coverage in encoded sRGB.

## Perspective

`fwidth(glyphCoord)` estimates the pixel footprint in em-space. Dual-axis rays scale intersection distances by `pixelsPerEm`.
