# Font Rendering Improvements

This document describes the modern font rendering improvements added to idtech3.

## Overview

The font rendering system has been significantly enhanced with modern features including:
- Kerning support
- UTF-8/Unicode support
- Font fallback chains
- Improved text measurement
- Font variation support (bold, italic)
- Enhanced FreeType integration
- Configurable rendering quality

## New Features

### 1. Kerning Support

Kerning adjusts spacing between character pairs for better readability (e.g., "AV", "To", "WA").

**Implementation:**
- Kerning data is pre-calculated during font registration
- Stored in `glyphInfo_t.kerning[]` array
- Applied automatically in improved text measurement functions
- Can be enabled/disabled via CVar

**CVar:** `r_fontKerning` (default: 1)
- `0` = Disabled
- `1` = Enabled

### 2. UTF-8/Unicode Support

Basic UTF-8 character decoding and support for international characters.

**Functions:**
- `RE_UTF8_CharLength()` - Get byte length of UTF-8 character
- `RE_UTF8_DecodeChar()` - Decode UTF-8 to Unicode code point

**Note:** Full Unicode glyph rendering requires additional glyph mapping infrastructure.

### 3. Font Fallback Chains

Support for font fallback when primary font doesn't have a glyph.

**Functions:**
- `RE_RegisterFontFallback()` - Register a fallback chain
- `RE_FindGlyphInFallback()` - Find glyph in fallback chain
- `RE_GetFontFallbackChain()` - Get fallback chain by name

### 4. Improved Text Measurement

Enhanced text measurement functions with kerning support.

**Functions:**
- `RE_Text_Width_Improved()` - Calculate text width with kerning
- `RE_Text_Height_Improved()` - Calculate text height
- `RE_Text_Bounds_Improved()` - Get both width and height

### 5. Font Variations (Bold/Italic)

Automatic detection and selection of font variations from font collections.

**Features:**
- Detects bold/italic from filename (e.g., "font_bold.ttf")
- Searches font collection for matching style
- Automatically selects appropriate face index

### 6. Enhanced FreeType Integration

Additional FreeType wrapper functions for advanced font operations.

**New Functions:**
- `FreeType_GetKerning()` - Get kerning between glyphs
- `FreeType_GetKerningDefault()` - Get kerning with default mode
- `FreeType_HasKerning()` - Check if font supports kerning
- `FreeType_GetCharWidth()` - Get character advance width
- `FreeType_GetCharHeight()` - Get character height
- `FreeType_GetFaceInfo()` - Get font face information

## Configuration CVars

All font quality CVars are archived and require a restart to take effect.

### `r_fontAtlasSize`
- **Default:** 256
- **Range:** 256, 512, or 1024
- **Description:** Font texture atlas size in pixels. Larger sizes allow more glyphs per texture but use more memory.

### `r_fontDPI`
- **Default:** 96
- **Range:** 72-300
- **Description:** DPI (dots per inch) for font rendering. Higher values produce sharper text but larger textures.
- **Typical values:**
  - 72 = Standard
  - 96 = Windows standard
  - 144 = Retina displays

### `r_fontHinting`
- **Default:** 2
- **Range:** 0-3
- **Description:** Font hinting mode
  - 0 = None
  - 1 = Light
  - 2 = Normal (default)
  - 3 = Strong
- Hinting improves text clarity at small sizes.

### `r_fontAntialiasing`
- **Default:** 1
- **Range:** 0-1
- **Description:** Enable font antialiasing
  - 0 = Disabled (monochrome)
  - 1 = Enabled (smooth)

### `r_fontLCDFilter`
- **Default:** 0
- **Range:** 0-1
- **Description:** Enable LCD subpixel filtering for improved text rendering on LCD displays. Requires antialiasing enabled.

### `r_fontKerning`
- **Default:** 1
- **Range:** 0-1
- **Description:** Enable font kerning for improved text spacing. Kerning adjusts spacing between character pairs for better readability.

## Data Structures

### Enhanced `glyphInfo_t`
```c
typedef struct {
  // ... existing fields ...
  int kerning[256]; // kerning offsets for character pairs
} glyphInfo_t;
```

### Enhanced `fontInfo_t`
```c
typedef struct {
  // ... existing fields ...
  qboolean hasKerning;      // whether this font supports kerning
  int pointSize;            // point size this font was rendered at
  float dpi;                // DPI used for rendering
  char familyName[64];      // font family name
  char styleName[64];       // font style name (Regular, Bold, Italic, etc.)
} fontInfo_t;
```

## Usage Examples

### Using Improved Text Measurement

```c
fontInfo_t *font = &cgDC.Assets.textFont;
float width = RE_Text_Width_Improved("Hello World", 1.0f, font, 0);
float height = RE_Text_Height_Improved("Hello World", 1.0f, font, 0);
```

### UTF-8 Character Decoding

```c
const unsigned char *str = (const unsigned char *)"Hello 世界";
unsigned int ch;
while (*str) {
    ch = RE_UTF8_DecodeChar(&str);
    if (ch == 0) break;
    // Process character...
}
```

### Font Fallback Chain

```c
const char *fallbacks[] = {"fonts/primary.ttf", "fonts/fallback.ttf"};
RE_RegisterFontFallback("fonts/main.ttf", 16, fallbacks, 2);
```

## Backward Compatibility

All improvements are backward compatible:
- Existing fonts continue to work
- Pre-rendered font data files are still supported
- Default settings match original behavior
- New features are opt-in via CVars

## Performance Considerations

- Kerning pre-calculation happens once during font registration
- UTF-8 decoding is lightweight
- Font fallback chains add minimal overhead
- Larger texture atlases use more memory but reduce texture switches

## Future Enhancements

Potential future improvements:
- Full Unicode glyph mapping
- Signed Distance Field (SDF) rendering for better scaling
- Multi-channel SDF (MSDF) for even better quality
- Dynamic font loading/unloading
- Font caching improvements
- Better LCD subpixel rendering

## Files Modified

- `src/renderercommon/tr_font.c` - Main font rendering
- `src/renderercommon/tr_font_utf8.c` - UTF-8 support (new)
- `src/renderercommon/tr_font_fallback.c` - Fallback chains (new)
- `src/common/freetype_wrapper.c` - Enhanced FreeType integration
- `src/common/q_shared.h` - Updated data structures
- `src/renderer/tr_init.c` - CVar registration
- `src/renderer2/tr_init.c` - CVar registration
- `src/renderervk/tr_init.c` - CVar registration
- Renderer header files - Function declarations

