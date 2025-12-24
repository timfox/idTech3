# Fonts Directory

This directory contains TrueType (.ttf) or OpenType (.otf) font files for use in mymod menus and HUD.

## Recommended Fonts

For best results, use modern, readable fonts:

### Free Fonts (Recommended)
- **Roboto** - Google's modern sans-serif font (excellent readability)
- **Open Sans** - Clean, professional sans-serif
- **Source Sans Pro** - Adobe's open-source font
- **Noto Sans** - Google's font with excellent Unicode support
- **Inter** - Modern UI font with excellent legibility

### Font Files
Place your font files here with descriptive names:
- `roboto-regular.ttf` - Main menu font
- `roboto-bold.ttf` - Bold text
- `roboto-italic.ttf` - Italic text

## Font Configuration

Fonts are configured in `fonts/fonts.cfg`. See that file for details.

## Font Sizes

Recommended point sizes:
- **Small font**: 12-14pt (for UI elements, lists)
- **Text font**: 16-18pt (for menu items, buttons)
- **Big font**: 24-28pt (for titles, headers)

## Font Rendering Quality

The engine supports advanced font rendering features:
- **Kerning**: Automatic character spacing adjustment
- **Antialiasing**: Smooth text edges
- **Hinting**: Optimized rendering at small sizes
- **DPI Scaling**: Automatic scaling based on screen resolution

Configure these via CVars:
- `r_fontDPI` - DPI for font rendering (default: 96)
- `r_fontKerning` - Enable kerning (default: 1)
- `r_fontAntialiasing` - Enable antialiasing (default: 1)
- `r_fontHinting` - Hinting mode (0=none, 1=light, 2=normal, 3=strong)
- `r_fontAtlasSize` - Texture atlas size (256, 512, or 1024)

## Font Fallback

If a character is not found in the primary font, the engine will automatically use fallback fonts. Configure fallback chains in `fonts/fonts.cfg`.

