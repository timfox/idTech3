# Fonts

This directory and `base/fonts/` hold TrueType fonts used by the renderer when `BUILD_FREETYPE` is enabled.

## Default: Inter

**Inter** (v4.1) is the default UI font. When `r_font` is set to `fonts/Inter-Regular.ttf` (the default), all UI text that requests `fonts/default` uses Inter.

- **Automatic download**: CMake downloads Inter to `base/fonts/` during configure when the font files are missing.
- **Files**: `Inter-Regular.ttf`, `Inter-Bold.ttf` (in `base/fonts/`)
- **License**: SIL Open Font License 1.1

## Custom fonts

- **r_font**: Set to a path like `fonts/MyFont.ttf` to override the default. Use `r_font ""` to fall back to the legacy bitmap font.
- **r_consoleFont**: Separate font for the console (e.g. `fonts/consolefont.ttf`).
- **r_fontSize**: Point size for custom fonts (default 16).
- **r_fontDpi**: FreeType device DPI for rasterization (default 72; try **96** for sharper glyphs when text is magnified on HiDPI displays). Clamped 72–144; restart after change.
- **r_fontHint**: **0** = legacy `FT_LOAD_DEFAULT`, **1** (default) = `FT_LOAD_TARGET_LIGHT`, **2** = `FT_LOAD_TARGET_NORMAL`. Restart after change.
- **r_fontMipmap**: **1** (default) builds mipmaps on each 256×256 TrueType atlas page for cleaner minification; **0** = single mip (legacy). After changing raster cvars, run **`reloadTtf`** in the client console or **`vid_restart`** (optional **`keep_window`**).

Place `.ttf` files in `base/fonts/` so the game can find them. The compile script copies `base/fonts/*.ttf` to the release directory.

## Console vs HUD

Engine **HUD** text uses FreeType when **`cl_builtInTtf`** is **1** (default) and **`r_font`** resolves.

**Console and notify** text defaults to the legacy **8×16 bitmap charset** (`cl_builtInTtfConsole` **0**, default) for a stable monospace grid. Optional paths:

| Mode | Cvars | Config preset |
|------|-------|---------------|
| Bitmap (stock-like) | `cl_builtInTtfConsole 0`, `r_sdfEnable 0` | `classic_baseq3.cfg` |
| FreeType console | `cl_builtInTtfConsole 1`, `r_fontShadow 0` | `exec console_ttf.cfg` |
| SDF console | `cl_builtInTtfConsole 0`, `r_sdfEnable 1`, valid `r_sdfFont` | `exec console_sdf.cfg` |

Draw order for console/notify: vector (if enabled) → SDF → optional FreeType (`cl_builtInTtfConsole 1`) → bitmap.

After changing font cvars, run **`reloadTtf`** or toggle **`cl_builtInTtfConsole`** (triggers console reflow) or **`vid_restart`**.

- **r_fontConsoleAlign** (**1** default) vertically aligns TrueType glyphs to a row baseline inside each cell; set **0** for legacy top alignment.
- **r_fontShadow** (**0–8**, default **2**) controls the TrueType drop shadow for HUD bigchars; console FreeType omits shadow. Use **`r_fontShadow 0`** with `console_ttf.cfg`.
- **r_fontSubpixel** **1** applies a small fractional nudge after projection (try on fuzzy LCDs). Empirical work on LCD subpixel preference: **`docs/research/bias2009-subpixel-preference.md`** (Bias et al., JASIST 2009). GPU/print RIP perspective: **`docs/research/recker2009-gpu-rip-fonts.md`** (Recker et al., HP Labs HPL-2009-181).

## SDF fonts (HUD)

Optional resolution-independent SDF text (`r_sdfEnable 1`): use `r_sdfFont` with a BMFont `.fnt` metrics file and matching atlas texture. Generate these from Inter (or any TTF) using tools like msdfgen, Hiero, or fontbm. To draw SDF instead of FreeType for those engine paths, set **`cl_builtInTtf`** to **0**.
