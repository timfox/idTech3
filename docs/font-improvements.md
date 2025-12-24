# Font and Menu Improvements for mymod

This document summarizes the font and menu improvements made to mymod.

## What Was Added

### 1. Font Directory Structure
- Created `mymod/fonts/` directory for TrueType/OpenType font files
- Added `fonts/README.md` with font recommendations and usage guide
- Added `fonts/fonts.cfg` configuration file for font setup
- Added `fonts/FONT_SETUP.md` comprehensive setup guide

### 2. Font Loading System
- Created `gamesrc/cgame/cg_fonts.c` with `CG_LoadFontConfig()` function
- Automatically loads fonts from `fonts/fonts.cfg` when menus initialize
- Supports font, smallFont, and bigFont declarations
- Supports font fallback chain configuration

### 3. Menu Improvements
- Updated `gamesrc/ui/ui_menu.c` to use bigger font for title (UI_BIGFONT)
- Improved menu styling with better font rendering
- Created `ui/hud_modern.txt` example HUD configuration with modern fonts

### 4. Integration
- Font loading is automatically called after menu loading in `CG_LoadHudMenu()`
- Fonts are registered using the existing `cgDC.registerFont` system
- Compatible with existing bitmap font fallback system

## How to Use

### Step 1: Add Font Files
1. Download free fonts (Roboto, Open Sans, Noto Sans, etc.)
2. Place `.ttf` or `.otf` files in `mymod/fonts/` directory
3. Name them descriptively (e.g., `roboto-regular.ttf`, `roboto-bold.ttf`)

### Step 2: Configure Fonts
Edit `mymod/fonts/fonts.cfg`:
```
font "fonts/roboto-regular.ttf" 16
smallFont "fonts/roboto-regular.ttf" 12
bigFont "fonts/roboto-bold.ttf" 24
```

### Step 3: Enable Modern HUD (Optional)
Set the HUD file to use modern fonts:
```
/cg_hudFiles ui/hud_modern.txt
```

### Step 4: Configure Font Rendering Quality
Adjust font rendering CVars for best quality:
```
/r_fontDPI 120          # Higher DPI for sharper text
/r_fontKerning 1        # Enable character spacing
/r_fontAntialiasing 1   # Smooth text edges
/r_fontHinting 2        # Normal hinting mode
/r_fontAtlasSize 512    # Larger atlas for better quality
```

## Features

### Font Rendering Quality
- **Kerning**: Automatic character spacing adjustment for better readability
- **Antialiasing**: Smooth text edges at all sizes
- **Hinting**: Optimized rendering for small text sizes
- **DPI Scaling**: Automatic scaling based on screen resolution
- **Texture Atlas**: Efficient glyph caching system

### Font Fallback
- Automatic fallback to bitmap fonts if TrueType fonts aren't available
- Configurable fallback chains for missing characters
- Unicode support for international characters

### Menu Improvements
- Better title rendering with larger fonts
- Improved text spacing and readability
- Modern font rendering throughout menus
- Consistent font usage across UI elements

## Technical Details

### Font Loading
Fonts are loaded from `fonts/fonts.cfg` using a simple parser that:
- Supports comments (// style)
- Parses font declarations (font, smallFont, bigFont)
- Supports font fallback chains
- Logs font loading to console

### Integration Points
- `CG_LoadFontConfig()` is called from `CG_LoadHudMenu()` in `cg_main.c`
- Fonts are registered using `cgDC.registerFont()` which calls `trap_R_RegisterFont()`
- The engine's FreeType integration handles actual font rendering

### Build System
- `cg_fonts.c` is automatically included via wildcard in CMakeLists.txt
- No manual build configuration needed
- Compatible with both CMake and Makefile builds

## Troubleshooting

### Fonts Not Loading
- Check console for font loading messages
- Verify font file paths in `fonts.cfg` are correct
- Ensure font files exist in `fonts/` directory
- Check that FreeType support is enabled in engine

### Poor Font Quality
- Increase `r_fontDPI` (try 120-144)
- Enable `r_fontAntialiasing`
- Try different hinting modes
- Increase `r_fontAtlasSize` to 512 or 1024

### Missing Characters
- Use fonts with good Unicode coverage (Noto Sans recommended)
- Configure font fallback chains
- Check font file supports required character sets

## Future Enhancements

Potential improvements for the future:
- Font variation support (bold, italic from single font file)
- Dynamic font size adjustment based on screen resolution
- Font caching system for faster loading
- Custom font rendering shaders
- Font outline/shadow effects
- Better Unicode/emoji support

## Files Modified/Created

### Created Files
- `mymod/fonts/README.md`
- `mymod/fonts/fonts.cfg`
- `mymod/fonts/FONT_SETUP.md`
- `mymod/gamesrc/cgame/cg_fonts.c`
- `mymod/ui/hud_modern.txt`
- `mymod/FONT_IMPROVEMENTS.md` (this file)

### Modified Files
- `mymod/gamesrc/cgame/cg_main.c` - Added font config loading
- `mymod/gamesrc/cgame/cg_local.h` - Added function declaration
- `mymod/gamesrc/ui/ui_menu.c` - Improved title rendering

## Credits

Font rendering improvements leverage the engine's FreeType integration with:
- Kerning support
- Antialiasing
- Hinting
- DPI scaling
- Texture atlas system

