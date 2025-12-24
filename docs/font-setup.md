# Font Setup Guide for mymod

This guide explains how to set up modern fonts for improved menu rendering.

## Quick Start

1. **Download Fonts**: Get free fonts like Roboto, Open Sans, or Noto Sans
2. **Place Fonts**: Put `.ttf` or `.otf` files in `mymod/fonts/` directory
3. **Configure**: Edit `fonts/fonts.cfg` to specify which fonts to use
4. **Enable**: Set `cg_hudFiles` to `ui/hud_modern.txt` (or your custom HUD file)

## Font Configuration

Edit `fonts/fonts.cfg`:

```
// Main menu font
font "fonts/roboto-regular.ttf" 16

// Small font for lists
smallFont "fonts/roboto-regular.ttf" 12

// Big font for titles
bigFont "fonts/roboto-bold.ttf" 24
```

## Font Rendering Quality

The engine supports advanced font rendering. Configure via CVars:

```bash
# Set DPI for font rendering (72-300, default: 96)
/r_fontDPI 120

# Enable kerning for better character spacing
/r_fontKerning 1

# Enable antialiasing for smooth edges
/r_fontAntialiasing 1

# Set hinting mode (0=none, 1=light, 2=normal, 3=strong)
/r_fontHinting 2

# Set texture atlas size (256, 512, or 1024)
/r_fontAtlasSize 512
```

## Recommended Fonts

### Free Fonts
- **Roboto** - Google's modern sans-serif (excellent readability)
- **Open Sans** - Clean, professional sans-serif
- **Source Sans Pro** - Adobe's open-source font
- **Noto Sans** - Excellent Unicode support
- **Inter** - Modern UI font

### Where to Get Fonts
- Google Fonts: https://fonts.google.com
- Font Squirrel: https://www.fontsquirrel.com
- Open Font Library: https://openfontlibrary.org

## Font Sizes

Recommended point sizes:
- **Small**: 12-14pt (lists, small UI elements)
- **Normal**: 16-18pt (menu items, buttons)
- **Big**: 24-28pt (titles, headers)

## Troubleshooting

### Fonts Not Loading
- Check that font files exist in `fonts/` directory
- Verify file paths in `fonts.cfg` are correct
- Ensure FreeType support is enabled in engine
- Check console for font loading messages

### Poor Font Quality
- Increase `r_fontDPI` for higher resolution rendering
- Enable `r_fontAntialiasing` for smoother edges
- Try different hinting modes with `r_fontHinting`
- Increase `r_fontAtlasSize` for better glyph quality

### Missing Characters
- Use fonts with good Unicode coverage (Noto Sans recommended)
- Configure font fallback chains in `fonts.cfg`
- Check that font file supports required character sets

## Advanced: Font Fallback

Configure fallback fonts for missing characters:

```
fontFallback "fonts/roboto-regular.ttf" 16 "fonts/noto-sans.ttf" "fonts/dejavu-sans.ttf"
```

This ensures that if a character isn't in Roboto, Noto Sans will be tried, then DejaVu Sans.

## Performance Tips

- Use smaller atlas sizes (256) for lower memory usage
- Disable kerning (`r_fontKerning 0`) for slightly better performance
- Use lighter hinting modes for faster rendering
- Pre-render fonts at specific sizes to avoid runtime generation

