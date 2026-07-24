# Vector Font Shaping

Unicode codepoints are not glyphs.

## Current state

- Primary draw path: UTF-8 decode → legacy 8-bit glyph table and FreeType-derived advance.
- `tr_vector_font_shape.c` provides `R_VectorFont_ShapeRun` with a HarfBuzz-ready API.
- The current `BUILD_HARFBUZZ` block is not a working shaping path: no
  `hb_font_t` is bound and `hb_shape` is not called.
- Complex scripts, ligatures, combining-mark placement, bidi runs, and
  glyph-index-only substitutions are therefore not supported by the
  analytical renderer yet. The legacy text renderer remains the production
  fallback.

## Planned HarfBuzz outputs

glyph index, cluster, x/y advance, x/y offset, script, direction, language, features.

## Cache policy

Cache shaped runs by font identity + string hash + script/language/direction/features/variation.
**Never** cache raster results.
