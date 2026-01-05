/*
===========================================================================
Fallback Assets System Header

Provides minimal fallback assets when game content is missing to prevent
black screen and provide user feedback.
===========================================================================
*/

#ifndef __Q_FALLBACK_ASSETS_H__
#define __Q_FALLBACK_ASSETS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "q_shared.h"

// Fallback asset types
typedef enum {
    FALLBACK_ASSET_FONT,
    FALLBACK_ASSET_TEXTURE,
    FALLBACK_ASSET_SHADER,
    FALLBACK_ASSET_SOUND
} fallback_asset_type_t;

// Fallback asset info structure
typedef struct {
    const char *name;              // Asset name/path
    fallback_asset_type_t type;    // Asset type
    const void *data;              // Asset data (for generated assets)
    size_t data_size;              // Size of data
    qboolean is_generated;         // True if asset is generated at runtime
} fallback_asset_t;

// Font asset structure for bitmap fonts
typedef struct {
    int width;           // Character width in pixels
    int height;          // Character height in pixels
    int char_count;      // Number of characters (ASCII 32-126)
    byte *bitmap;        // Font bitmap data (width * height * char_count)
} fallback_font_t;

// Texture asset structure
typedef struct {
    int width;
    int height;
    int components;      // 3 = RGB, 4 = RGBA
    byte *pixels;        // Texture pixel data
} fallback_texture_t;

// Public API functions
qboolean FS_LoadFallbackAssets(void);
void FS_GenerateFallbackFont(void);
void FS_GenerateFallbackTextures(void);
qboolean FS_IsFallbackAssetLoaded(const char *name);
const fallback_asset_t *FS_GetFallbackAsset(const char *name);

// Font rendering helpers
qboolean FallbackFont_IsAvailable(void);
void FallbackFont_RenderChar(int x, int y, char c, const vec4_t color);
void FallbackFont_RenderText(int x, int y, const char *text, const vec4_t color);

// Texture helpers
qhandle_t FallbackTexture_Load(const char *name);
qboolean FallbackTexture_IsAvailable(const char *name);

// Error display functions
void FallbackAssets_DisplayErrorMessage(const char *title, const char *message);
void FallbackAssets_DisplayAssetInstructions(void);

#ifdef __cplusplus
}
#endif

#endif // __Q_FALLBACK_ASSETS_H__