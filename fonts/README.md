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

Place `.ttf` files in `base/fonts/` so the game can find them. The compile script copies `base/fonts/*.ttf` to the release directory.

## SDF fonts (HUD)

For resolution-independent SDF text (`r_sdfEnable 1`), use `r_sdfFont` with a BMFont `.fnt` metrics file and matching atlas texture. Generate these from Inter (or any TTF) using tools like msdfgen, Hiero, or fontbm.
