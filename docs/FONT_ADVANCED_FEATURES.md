# Advanced Font Rendering Features

## Overview

This document describes the advanced font rendering features that have been implemented:
- **Font Caching** - Avoids reloading fonts across level changes
- **Font Fallback Chain Integration** - Automatically uses fallback fonts for missing glyphs
- **Full Unicode Rendering** - Complete Unicode glyph mapping support

## 1. Font Caching System

### Implementation

Fonts are now cached by **name + point size** combination, allowing:
- Faster font loading on subsequent requests
- Fonts persist across level changes (cache is not cleared)
- Better performance when multiple fonts are used

### Cache Structure

```c
typedef struct {
    char fontName[MAX_QPATH];
    int pointSize;
    fontInfo_t *font;
    qboolean inUse;
} fontCacheEntry_t;
```

### Cache Size

- Maximum cache size: **32 fonts** (`MAX_FONT_CACHE`)
- Cache persists across level changes for better performance
- Cache can be cleared manually if needed (via `R_DoneFreeType`)

### Benefits

- **Faster loading**: Fonts loaded once are reused instantly
- **Memory efficient**: Only caches fonts actually used
- **Level persistence**: Fonts don't reload when changing levels

## 2. Font Fallback Chain Integration

### Implementation

Font fallback chains are now **automatically used** when rendering text. If a glyph is missing in the primary font, the system checks the fallback font chain.

### How It Works

1. **Primary font** is checked first for the glyph
2. If glyph is missing (`glyph->glyph == 0`), check `font->fallbackFont`
3. Fallback font is checked recursively
4. If found in fallback, use that glyph instead

### Integration Points

Fallback chain checking is integrated into:
- `RE_Text_Width_Improved()` - Text width calculation
- `RE_Text_Height_Improved()` - Text height calculation  
- `RE_Text_Paint_Improved()` - Text rendering
- `RE_Text_Paint_3D_Improved()` - 3D text rendering

### Font Structure

```c
struct fontInfo_s {
    // ... existing fields ...
    fontInfo_t *fallbackFont; // Linked list of fallback fonts
};
```

### Usage

When registering a font fallback chain:
```c
const char *fallbacks[] = {"fonts/noto-sans.ttf", "fonts/noto-emoji.ttf"};
RE_RegisterFontFallback("fonts/roboto.ttf", 18, fallbacks, 2);
```

The first fallback font is automatically linked to the primary font's `fallbackFont` pointer.

## 3. Full Unicode Rendering

### Implementation

Complete Unicode glyph mapping support with:
- Unicode code point to glyph index mapping
- Cache for frequently used Unicode characters
- Recursive fallback chain checking for Unicode characters

### Unicode Functions

#### `RE_GetGlyphIndexForUnicode()`
Get glyph index for a Unicode code point.

```c
unsigned int RE_GetGlyphIndexForUnicode(fontInfo_t *font, unsigned int codePoint);
```

#### `RE_FindUnicodeGlyphInFont()`
Find a Unicode glyph in a font, checking fallback chain if needed.

```c
glyphInfo_t *RE_FindUnicodeGlyphInFont(fontInfo_t *font, unsigned int codePoint);
```

#### `RE_CacheUnicodeGlyph()`
Cache a Unicode code point to glyph index mapping.

```c
void RE_CacheUnicodeGlyph(unsigned int codePoint, unsigned int glyphIndex);
```

#### `RE_ClearUnicodeCache()`
Clear the Unicode glyph cache.

```c
void RE_ClearUnicodeCache(void);
```

### Unicode Cache

- Maximum cache size: **1024 entries** (`MAX_UNICODE_CACHE`)
- Caches Unicode code point → glyph index mappings
- Improves performance for frequently used Unicode characters

### UTF-8 Integration

Unicode support is fully integrated into UTF-8 text rendering:

1. **Decode UTF-8** to Unicode code point
2. **Check Unicode cache** for glyph index
3. **Find glyph** in font (with fallback chain support)
4. **Render glyph** if found, skip if not

### Example

```c
// Text with Unicode characters
const char *text = "Hello 世界 🌍";

// Render with Unicode support
RE_Text_Paint_Improved(x, y, scale, color, text, adjust, limit, style, font);
```

The system will:
1. Decode "世" (U+4E16) and "界" (U+754C) from UTF-8
2. Look up glyphs in font (with fallback chain)
3. Render Unicode characters if glyphs are found
4. Skip characters if no glyph is available

## Technical Details

### File Structure

- `src/renderercommon/tr_font.c` - Font caching implementation
- `src/renderercommon/tr_font_utf8.c` - UTF-8 and fallback integration
- `src/renderercommon/tr_font_unicode.c` - Unicode glyph mapping
- `src/renderercommon/tr_font_fallback.c` - Fallback chain management

### Memory Usage

- **Font cache**: ~32 entries × ~8KB = ~256KB
- **Unicode cache**: 1024 entries × 8 bytes = ~8KB
- **Total overhead**: ~264KB (negligible for modern systems)

### Performance Impact

- **Font caching**: Eliminates font reload overhead (significant improvement)
- **Fallback chains**: Minimal overhead (only checked when glyph missing)
- **Unicode cache**: Reduces FreeType lookups for repeated characters

## Usage Examples

### Example 1: Font with Fallback

```c
// Register font with fallback
const char *fallbacks[] = {"fonts/noto-sans.ttf"};
RE_RegisterFontFallback("fonts/roboto.ttf", 18, fallbacks, 1);

// Use font - fallback happens automatically
fontInfo_t font;
RE_RegisterFont("fonts/roboto.ttf", 18, &font);
// font.fallbackFont is automatically set to Noto Sans
```

### Example 2: Unicode Text Rendering

```c
// Text with Unicode characters
const char *text = "Hello 世界";

// Render - Unicode is handled automatically
RE_Text_Paint_Improved(x, y, 1.0f, color, text, 0, 0, 0, &font);
```

### Example 3: Font Caching

```c
// First load - font is loaded and cached
fontInfo_t font1;
RE_RegisterFont("fonts/roboto.ttf", 18, &font1);

// Second load - font is retrieved from cache (instant)
fontInfo_t font2;
RE_RegisterFont("fonts/roboto.ttf", 18, &font2);
// font2 is identical to font1, loaded from cache
```

## Future Enhancements

Potential improvements:
1. **LRU cache eviction** - Remove least recently used fonts when cache is full
2. **Unicode range optimization** - Pre-load common Unicode ranges
3. **Glyph preloading** - Load commonly used Unicode glyphs at font registration
4. **Cache statistics** - Track cache hit/miss rates for optimization

## Backward Compatibility

All features are **fully backward compatible**:
- Existing fonts continue to work without changes
- Fallback chains are optional (fonts work without them)
- Unicode support is automatic (ASCII still works as before)
- Cache is transparent (no API changes required)

## Testing

To test these features:

1. **Font caching**: Load same font twice, second load should be instant
2. **Fallback chains**: Use font missing some characters, verify fallback works
3. **Unicode**: Render text with Unicode characters, verify they appear correctly

## Summary

These advanced features provide:
- ✅ **Better performance** - Font caching eliminates reloads
- ✅ **Better coverage** - Fallback chains handle missing characters
- ✅ **International support** - Full Unicode rendering support
- ✅ **Backward compatible** - All existing code continues to work

