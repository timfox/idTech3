# Complete Font Setup Guide for mymod

## Quick Start (5 Minutes)

### Step 1: Download Fonts
1. Go to [Google Fonts](https://fonts.google.com/)
2. Download **Roboto** (Regular + Bold) or **Noto Sans** (Regular + Bold)
3. Extract `.ttf` files

### Step 2: Place Fonts
```
mymod/
└── fonts/
    ├── roboto-regular.ttf
    ├── roboto-bold.ttf
    └── fonts.cfg
```

### Step 3: Configure
Edit `mymod/fonts/fonts.cfg`:
```
font "fonts/roboto-regular.ttf" 18
smallFont "fonts/roboto-regular.ttf" 14
bigFont "fonts/roboto-bold.ttf" 24
```

### Step 4: Set Quality
Add to `mymod.cfg` or `autoexec.cfg`:
```
set r_fontDPI 120
set r_fontKerning 1
set r_fontAntialiasing 1
set r_fontAtlasSize 512
```

### Step 5: Test
Launch: `./quake3e +set fs_game mymod`
Check console for "Loaded font" messages.

## Detailed Configuration

### Font Sizes Guide

| Use Case | Recommended Size | Example |
|----------|------------------|---------|
| Menu titles | 24-32pt | Big, bold headers |
| Menu items | 16-20pt | Navigation, buttons |
| HUD text | 14-18pt | Health, ammo, scores |
| Small text | 12-14pt | Lists, tooltips |
| Console | 12-14pt | Debug output |

### Font Quality Settings

#### DPI (r_fontDPI)
- **72** - Standard (default)
- **96** - Windows standard
- **120** - High DPI displays (recommended)
- **144** - Very high DPI (may use more memory)

#### Atlas Size (r_fontAtlasSize)
- **256** - Low memory, good for small fonts
- **512** - Balanced (recommended)
- **1024** - High quality, more memory

#### Hinting (r_fontHinting)
- **0** - None (smooth but blurry at small sizes)
- **1** - Light (subtle optimization)
- **2** - Normal (recommended)
- **3** - Strong (sharp but may look distorted)

#### Antialiasing (r_fontAntialiasing)
- **0** - Disabled (pixelated)
- **1** - Enabled (recommended, smooth)

#### Kerning (r_fontKerning)
- **0** - Disabled (uniform spacing)
- **1** - Enabled (recommended, better readability)

## Font Recommendations

### For Modern Look
- **Roboto** - Clean, modern, excellent readability
- **Open Sans** - Professional, versatile
- **Lato** - Friendly, approachable

### For International Support
- **Noto Sans** - Best Unicode coverage (CJK, Arabic, etc.)
- **Noto Emoji** - For emoji support
- **DejaVu Sans** - Good fallback option

### For Retro/Classic Look
- **DejaVu Sans** - Classic terminal feel
- **Liberation Sans** - Windows-like
- **Ubuntu** - Linux-style

## Advanced: Font Fallback Chains

When full fallback support is implemented, you can use:

```
fontFallback "fonts/roboto-regular.ttf" 18 "fonts/noto-sans.ttf" "fonts/noto-emoji.ttf"
```

This ensures:
1. Try Roboto first
2. If character missing, try Noto Sans
3. If still missing, try Noto Emoji
4. Finally fall back to bitmap fonts

## Troubleshooting

### Fonts Not Loading
**Symptoms:** Console shows "Fonts config not found" or no font messages

**Solutions:**
1. Check `fonts/fonts.cfg` exists
2. Verify file paths are correct (relative to mod root)
3. Check font files exist in `fonts/` directory
4. Ensure FreeType is enabled in engine build

### Poor Font Quality
**Symptoms:** Text looks blurry or pixelated

**Solutions:**
1. Increase `r_fontDPI` to 120-144
2. Enable `r_fontAntialiasing`
3. Increase `r_fontAtlasSize` to 512 or 1024
4. Try different hinting modes (0-3)

### Missing Characters
**Symptoms:** Some characters show as boxes or missing

**Solutions:**
1. Use fonts with better Unicode coverage (Noto Sans)
2. Configure fallback chains (when implemented)
3. Check font file supports required character sets
4. Verify font file isn't corrupted

### Performance Issues
**Symptoms:** Slow menu loading or frame drops

**Solutions:**
1. Reduce `r_fontAtlasSize` to 256
2. Lower `r_fontDPI` to 96
3. Disable `r_fontAntialiasing` (if needed)
4. Don't load too many fonts

## Best Practices

1. **Use appropriate font sizes** - Don't go too large (wastes memory)
2. **Test on target resolution** - Fonts look different at different sizes
3. **Keep font count low** - Each font uses memory
4. **Use system fonts when possible** - Better compatibility
5. **Include font licenses** - Required for distribution
6. **Test fallback behavior** - Ensure bitmap fonts work as backup

## Performance Tips

- **Atlas size 512** is usually best balance
- **DPI 120** works well for most displays
- **Kerning** has minimal performance impact
- **Antialiasing** slightly impacts performance but worth it
- **Multiple fonts** each use memory - keep count reasonable

## Font Licensing

**Always check licenses before distributing:**

- **Roboto** - Apache 2.0 ✅ Free to use
- **Noto Sans** - SIL OFL ✅ Free to use  
- **Open Sans** - Apache 2.0 ✅ Free to use
- **DejaVu Sans** - Public Domain ✅ Free to use

**Include license files** when distributing your mod!

## Example Configurations

### Minimal Setup
```
font "fonts/roboto-regular.ttf" 16
smallFont "fonts/roboto-regular.ttf" 12
bigFont "fonts/roboto-bold.ttf" 20
```

### High Quality Setup
```
font "fonts/noto-sans-regular.ttf" 18
smallFont "fonts/noto-sans-regular.ttf" 14
bigFont "fonts/noto-sans-bold.ttf" 24

fontFallback "fonts/noto-sans-regular.ttf" 18 "fonts/noto-emoji.ttf"
```

### Performance-Optimized Setup
```
font "fonts/roboto-regular.ttf" 16
smallFont "fonts/roboto-regular.ttf" 12
bigFont "fonts/roboto-bold.ttf" 20

// In config:
set r_fontDPI 96
set r_fontAtlasSize 256
set r_fontAntialiasing 1
```

## Next Steps

1. ✅ Download fonts
2. ✅ Configure `fonts.cfg`
3. ✅ Set quality CVars
4. ✅ Test in game
5. ✅ Adjust sizes as needed
6. ✅ Document your font choices

## Support

If you encounter issues:
1. Check console for error messages
2. Verify font files aren't corrupted
3. Test with default fonts first
4. Check engine FreeType support
5. Review this guide for common issues

