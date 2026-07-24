# Vector Font Rendering

**Status:** Experimental Lengyel path (JCGT 2017). Dual-axis sorted curve
lists, analytical coverage modes, and premultiplied blending are implemented,
but the path is not production-certified. Run `vector_font_certify` for the
current blockers.

**Primary reference:** Eric Lengyel, “GPU-Centered Font Rendering Directly from Glyph Outlines,” JCGT 2017.

**Foundational:** Loop & Blinn 2005 (mode 2 glyphlets).

## Atlas-free meaning

No raster glyph / SDF / MSDF atlas. Persistent GPU storage of **curve control points** and **dual sorted lists** (X for horizontal rays, Y for vertical) is required and reported by `vector_font_memory_status`.

## Quick start

```text
set r_font fonts/Inter-Regular.ttf
set r_vectorFont 1
set r_vectorFontCoverage 2
reloadTtf
```

## Cvars

| Cvar | Default | Role |
|------|---------|------|
| `r_vectorFont` | 0 | Enable GPU outline text |
| `r_vectorFontMode` | 0 | 0=Lengyel, 2=Loop&Blinn |
| `r_vectorFontCoverage` | 2 | 0 center, 1 dual-axis, 2 adaptive SS, 3 ultra |
| `r_vectorFontHinting` | 0 | 0 unhinted, 1 light, 2 native UI |
| `r_vectorFontStemDarkening` | 0 | Optical stem darkening strength |
| `r_vectorFontPixelSnap` | 0 | 0 fractional, 1 baseline, 2 baseline+origin (UI only) |
| `r_vectorFontDebug` | 0 | Debug views 0–15 |

## Pipeline

```text
UTF-8 legacy fallback → FreeType outline → quadratic curves
→ X-sorted + Y-sorted lists → curve texture → glyph quads
→ Lengyel winding + coverage → premultiplied ui_overlay (post TAA)
```

## Cubic policy

CFF cubics currently convert to three quadratics via a fixed midpoint split.
This conversion has no adaptive error bound and is not certified. Native cubic
GPU evaluation is not yet enabled.

## Resource policy

Curve texture staging grows geometrically with actual outline complexity. It
starts at 4096 RGBA texels and is capped at 16 million texels, instead of
reserving the worst-case curve count for every legacy glyph slot. The X- and
Y-sorted lists intentionally duplicate vector control points.

## Certification blockers

- HarfBuzz face/font caching and glyph-index outline caching
- bounded-error cubic conversion
- hinted per-ppem vector outline caches
- COLR/CPAL vector layers and fallback reporting
- high-resolution numerical reference comparisons
- world-space projective and temporal-stability evidence

## Temporal policy

HUD/console vector text draws into `ui_overlay` and is composed **after** world TAA / tonemap. It does not enter TAA history.

## Commands

```text
vector_font_status
vector_font_validate
vector_font_memory_status
vector_font_gpu_status
vector_font_certify
```

## Related

- [VECTOR_FONT_COVERAGE.md](VECTOR_FONT_COVERAGE.md)
- [VECTOR_FONT_WINDING.md](VECTOR_FONT_WINDING.md)
- [VECTOR_FONT_SHAPING.md](VECTOR_FONT_SHAPING.md)
- Legacy detail: older `VECTOR_FONT.md` mode-2 notes remain valid for Loop & Blinn.
