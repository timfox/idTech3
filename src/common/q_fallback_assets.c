/*
===========================================================================
Fallback Assets System Implementation

Provides minimal fallback assets when game content is missing to prevent
black screen and provide user feedback.
===========================================================================
*/

#include "q_fallback_assets.h"
#include "qcommon.h"
#include "files.h"
#include <string.h>
#include <stdlib.h>

// Maximum number of fallback assets
#define MAX_FALLBACK_ASSETS 32

// Fallback asset registry
static fallback_asset_t fallback_assets[MAX_FALLBACK_ASSETS];
static int num_fallback_assets = 0;

// Generated assets storage
static fallback_font_t fallback_font;
static fallback_texture_t fallback_console_bg;
static fallback_texture_t fallback_error_icon;

// Font bitmap data - minimal 8x8 ASCII font for characters 32-126 (95 characters)
// This is a simple fixed-width font generated programmatically
static byte font_bitmap[8 * 8 * 95]; // 8x8 pixels per character, 95 characters

// Texture data for simple placeholders
static byte console_bg_pixels[64 * 64 * 3]; // 64x64 RGB console background
static byte error_icon_pixels[32 * 32 * 4]; // 32x32 RGBA error icon

/*
=================
FallbackAssets_RegisterAsset

Register a fallback asset in the registry
=================
*/
static qboolean FallbackAssets_RegisterAsset(const char *name, fallback_asset_type_t type,
                                           const void *data, size_t data_size,
                                           qboolean is_generated) {
    if (num_fallback_assets >= MAX_FALLBACK_ASSETS) {
        Com_Printf("WARNING: Too many fallback assets registered\n");
        return qfalse;
    }

    fallback_asset_t *asset = &fallback_assets[num_fallback_assets++];
    asset->name = name;
    asset->type = type;
    asset->data = data;
    asset->data_size = data_size;
    asset->is_generated = is_generated;

    return qtrue;
}

/*
=================
FallbackAssets_GenerateFontBitmap

Generate a minimal 8x8 bitmap font for ASCII characters 32-126
=================
*/
static void FallbackAssets_GenerateFontBitmap(void) {
    // Simple 8x8 font patterns for basic ASCII characters
    // Each character is represented by 8 bytes (8x8 bitmap)
    static const byte font_patterns[95][8] = {
        // Space (32)
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        // ! (33)
        {0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x00},
        // " (34)
        {0x6C, 0x6C, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00},
        // # (35)
        {0x24, 0x24, 0x7E, 0x24, 0x7E, 0x24, 0x24, 0x00},
        // $ (36)
        {0x18, 0x3E, 0x60, 0x3C, 0x06, 0x7C, 0x18, 0x00},
        // % (37)
        {0x62, 0x64, 0x08, 0x10, 0x26, 0x46, 0x00, 0x00},
        // & (38)
        {0x30, 0x48, 0x48, 0x30, 0x4A, 0x44, 0x3A, 0x00},
        // ' (39)
        {0x18, 0x18, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00},
        // ( (40)
        {0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06, 0x00},
        // ) (41)
        {0x60, 0x30, 0x18, 0x18, 0x18, 0x30, 0x60, 0x00},
        // * (42)
        {0x00, 0x24, 0x18, 0x7E, 0x18, 0x24, 0x00, 0x00},
        // + (43)
        {0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00},
        // , (44)
        {0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x10, 0x00},
        // - (45)
        {0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00},
        // . (46)
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00},
        // / (47)
        {0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x00},
        // 0 (48)
        {0x3C, 0x66, 0x6E, 0x7E, 0x76, 0x66, 0x3C, 0x00},
        // 1 (49)
        {0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00},
        // 2 (50)
        {0x3C, 0x66, 0x06, 0x1C, 0x30, 0x60, 0x7E, 0x00},
        // 3 (51)
        {0x3C, 0x66, 0x06, 0x1C, 0x06, 0x66, 0x3C, 0x00},
        // 4 (52)
        {0x0C, 0x1C, 0x2C, 0x4C, 0x7E, 0x0C, 0x0C, 0x00},
        // 5 (53)
        {0x7E, 0x60, 0x7C, 0x06, 0x06, 0x66, 0x3C, 0x00},
        // 6 (54)
        {0x1C, 0x30, 0x60, 0x7C, 0x66, 0x66, 0x3C, 0x00},
        // 7 (55)
        {0x7E, 0x06, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x00},
        // 8 (56)
        {0x3C, 0x66, 0x66, 0x3C, 0x66, 0x66, 0x3C, 0x00},
        // 9 (57)
        {0x3C, 0x66, 0x66, 0x3E, 0x06, 0x0C, 0x38, 0x00},
        // : (58)
        {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00},
        // ; (59)
        {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x10, 0x00},
        // < (60)
        {0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00},
        // = (61)
        {0x00, 0x00, 0x7E, 0x00, 0x7E, 0x00, 0x00, 0x00},
        // > (62)
        {0x60, 0x30, 0x18, 0x0C, 0x18, 0x30, 0x60, 0x00},
        // ? (63)
        {0x3C, 0x66, 0x06, 0x0C, 0x18, 0x00, 0x18, 0x00},
        // @ (64)
        {0x3C, 0x66, 0x6E, 0x6E, 0x60, 0x66, 0x3C, 0x00},
        // A (65)
        {0x18, 0x3C, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x00},
        // B (66)
        {0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x7C, 0x00},
        // C (67)
        {0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C, 0x00},
        // D (68)
        {0x78, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0x78, 0x00},
        // E (69)
        {0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x7E, 0x00},
        // F (70)
        {0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x60, 0x00},
        // G (71)
        {0x3C, 0x66, 0x60, 0x6E, 0x66, 0x66, 0x3C, 0x00},
        // H (72)
        {0x66, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00},
        // I (73)
        {0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00},
        // J (74)
        {0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x6C, 0x38, 0x00},
        // K (75)
        {0x66, 0x6C, 0x78, 0x70, 0x78, 0x6C, 0x66, 0x00},
        // L (76)
        {0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00},
        // M (77)
        {0x66, 0x7E, 0x7E, 0x7E, 0x66, 0x66, 0x66, 0x00},
        // N (78)
        {0x66, 0x76, 0x7E, 0x7E, 0x6E, 0x66, 0x66, 0x00},
        // O (79)
        {0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00},
        // P (80)
        {0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x00},
        // Q (81)
        {0x3C, 0x66, 0x66, 0x66, 0x6E, 0x66, 0x3E, 0x00},
        // R (82)
        {0x7C, 0x66, 0x66, 0x7C, 0x78, 0x6C, 0x66, 0x00},
        // S (83)
        {0x3C, 0x66, 0x60, 0x3C, 0x06, 0x66, 0x3C, 0x00},
        // T (84)
        {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00},
        // U (85)
        {0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00},
        // V (86)
        {0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00},
        // W (87)
        {0x66, 0x66, 0x66, 0x7E, 0x7E, 0x7E, 0x66, 0x00},
        // X (88)
        {0x66, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00},
        // Y (89)
        {0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x00},
        // Z (90)
        {0x7E, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x7E, 0x00},
        // [ (91)
        {0x3C, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3C, 0x00},
        // \ (92)
        {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x00},
        // ] (93)
        {0x3C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3C, 0x00},
        // ^ (94)
        {0x18, 0x3C, 0x66, 0x42, 0x00, 0x00, 0x00, 0x00},
        // _ (95)
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7E, 0x00},
        // ` (96)
        {0x30, 0x18, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00},
        // a (97)
        {0x00, 0x00, 0x3C, 0x06, 0x3E, 0x66, 0x3E, 0x00},
        // b (98)
        {0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x7C, 0x00},
        // c (99)
        {0x00, 0x00, 0x3C, 0x60, 0x60, 0x66, 0x3C, 0x00},
        // d (100)
        {0x06, 0x06, 0x3E, 0x66, 0x66, 0x66, 0x3E, 0x00},
        // e (101)
        {0x00, 0x00, 0x3C, 0x66, 0x7E, 0x60, 0x3C, 0x00},
        // f (102)
        {0x1C, 0x36, 0x30, 0x7C, 0x30, 0x30, 0x30, 0x00},
        // g (103)
        {0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x3C, 0x00},
        // h (104)
        {0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00},
        // i (105)
        {0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3C, 0x00},
        // j (106)
        {0x06, 0x00, 0x0E, 0x06, 0x06, 0x66, 0x3C, 0x00},
        // k (107)
        {0x60, 0x60, 0x66, 0x6C, 0x78, 0x6C, 0x66, 0x00},
        // l (108)
        {0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00},
        // m (109)
        {0x00, 0x00, 0x6C, 0x7E, 0x7E, 0x66, 0x66, 0x00},
        // n (110)
        {0x00, 0x00, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00},
        // o (111)
        {0x00, 0x00, 0x3C, 0x66, 0x66, 0x66, 0x3C, 0x00},
        // p (112)
        {0x00, 0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x00},
        // q (113)
        {0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x00},
        // r (114)
        {0x00, 0x00, 0x6E, 0x70, 0x60, 0x60, 0x60, 0x00},
        // s (115)
        {0x00, 0x00, 0x3E, 0x60, 0x3C, 0x06, 0x7C, 0x00},
        // t (116)
        {0x30, 0x30, 0x7C, 0x30, 0x30, 0x30, 0x1C, 0x00},
        // u (117)
        {0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x00},
        // v (118)
        {0x00, 0x00, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00},
        // w (119)
        {0x00, 0x00, 0x66, 0x66, 0x7E, 0x7E, 0x66, 0x00},
        // x (120)
        {0x00, 0x00, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x00},
        // y (121)
        {0x00, 0x66, 0x66, 0x66, 0x3E, 0x06, 0x3C, 0x00},
        // z (122)
        {0x00, 0x00, 0x7E, 0x0C, 0x18, 0x30, 0x7E, 0x00},
        // { (123)
        {0x0E, 0x18, 0x18, 0x70, 0x18, 0x18, 0x0E, 0x00},
        // | (124)
        {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00},
        // } (125)
        {0x70, 0x18, 0x18, 0x0E, 0x18, 0x18, 0x70, 0x00},
        // ~ (126)
        {0x00, 0x00, 0x00, 0x36, 0x6C, 0x00, 0x00, 0x00}
    };

    // Copy font patterns to bitmap
    for (int i = 0; i < 95; i++) {
        memcpy(&font_bitmap[i * 8], font_patterns[i], 8);
    }
}

/*
=================
FallbackAssets_GenerateTextures

Generate simple placeholder textures
=================
*/
static void FallbackAssets_GenerateTextures(void) {
    int i, x, y;

    // Generate console background texture (64x64 RGB)
    // Simple gradient from dark blue to black
    for (y = 0; y < 64; y++) {
        for (x = 0; x < 64; x++) {
            int idx = (y * 64 + x) * 3;
            float t = (float)y / 63.0f; // 0 to 1
            console_bg_pixels[idx] = (byte)(20 * (1.0f - t));     // R
            console_bg_pixels[idx + 1] = (byte)(20 * (1.0f - t)); // G
            console_bg_pixels[idx + 2] = (byte)(40 * (1.0f - t)); // B
        }
    }

    // Generate error icon texture (32x32 RGBA)
    // Simple red warning triangle
    memset(error_icon_pixels, 0, sizeof(error_icon_pixels));

    for (y = 0; y < 32; y++) {
        for (x = 0; x < 32; x++) {
            int idx = (y * 32 + x) * 4;

            // Create a triangle shape
            if (x >= (32 - y * 2) / 2 && x < (32 + y * 2) / 2 && y < 24) {
                error_icon_pixels[idx] = 255;     // R
                error_icon_pixels[idx + 1] = 0;   // G
                error_icon_pixels[idx + 2] = 0;   // B
                error_icon_pixels[idx + 3] = 255; // A
            } else {
                error_icon_pixels[idx + 3] = 0;   // Transparent
            }
        }
    }
}

/*
=================
FS_GenerateFallbackFont

Generate the fallback font bitmap
=================
*/
void FS_GenerateFallbackFont(void) {
    Com_Printf("Generating fallback font...\n");

    FallbackAssets_GenerateFontBitmap();

    // Initialize font structure
    fallback_font.width = 8;
    fallback_font.height = 8;
    fallback_font.char_count = 95;
    fallback_font.bitmap = font_bitmap;

    // Register font asset
    FallbackAssets_RegisterAsset("menu/art/font1_prop.tga", FALLBACK_ASSET_FONT,
                               &fallback_font, sizeof(fallback_font_t), qtrue);

    Com_Printf("Fallback font generated\n");
}

/*
=================
FS_GenerateFallbackTextures

Generate simple placeholder textures
=================
*/
void FS_GenerateFallbackTextures(void) {
    Com_Printf("Generating fallback textures...\n");

    FallbackAssets_GenerateTextures();

    // Initialize texture structures
    fallback_console_bg.width = 64;
    fallback_console_bg.height = 64;
    fallback_console_bg.components = 3;
    fallback_console_bg.pixels = console_bg_pixels;

    fallback_error_icon.width = 32;
    fallback_error_icon.height = 32;
    fallback_error_icon.components = 4;
    fallback_error_icon.pixels = error_icon_pixels;

    // Register texture assets
    FallbackAssets_RegisterAsset("menu/art/back_0.tga", FALLBACK_ASSET_TEXTURE,
                               &fallback_console_bg, sizeof(fallback_texture_t), qtrue);
    FallbackAssets_RegisterAsset("menu/art/error_icon.tga", FALLBACK_ASSET_TEXTURE,
                               &fallback_error_icon, sizeof(fallback_texture_t), qtrue);

    Com_Printf("Fallback textures generated\n");
}

/*
=================
FS_LoadFallbackAssets

Initialize and load all fallback assets
=================
*/
qboolean FS_LoadFallbackAssets(void) {
    Com_Printf("Loading fallback assets...\n");

    // Generate font
    FS_GenerateFallbackFont();

    // Generate textures
    FS_GenerateFallbackTextures();

    Com_Printf("Fallback assets loaded (%d assets)\n", num_fallback_assets);
    return qtrue;
}

/*
=================
FS_IsFallbackAssetLoaded

Check if a fallback asset is available
=================
*/
qboolean FS_IsFallbackAssetLoaded(const char *name) {
    for (int i = 0; i < num_fallback_assets; i++) {
        if (Q_stricmp(fallback_assets[i].name, name) == 0) {
            return qtrue;
        }
    }
    return qfalse;
}

/*
=================
FS_GetFallbackAsset

Get fallback asset information
=================
*/
const fallback_asset_t *FS_GetFallbackAsset(const char *name) {
    for (int i = 0; i < num_fallback_assets; i++) {
        if (Q_stricmp(fallback_assets[i].name, name) == 0) {
            return &fallback_assets[i];
        }
    }
    return NULL;
}

/*
=================
FallbackFont_IsAvailable

Check if fallback font is available
=================
*/
qboolean FallbackFont_IsAvailable(void) {
    return fallback_font.bitmap != NULL;
}

/*
=================
FallbackFont_RenderChar

Render a single character using the fallback font
=================
*/
void FallbackFont_RenderChar(int x, int y, char c, const vec4_t color) {
    if (!FallbackFont_IsAvailable()) {
        return;
    }

    if (c < 32 || c > 126) {
        c = '?'; // Replace invalid chars with ?
    }

    int char_index = c - 32;
    if (char_index >= fallback_font.char_count) {
        return;
    }

    // This would need integration with the renderer to actually draw pixels
    // For now, just log that we would render
    Com_Printf("FallbackFont: Would render '%c' at (%d,%d)\n", c, x, y);
}

/*
=================
FallbackFont_RenderText

Render text using the fallback font
=================
*/
void FallbackFont_RenderText(int x, int y, const char *text, const vec4_t color) {
    if (!text || !FallbackFont_IsAvailable()) {
        return;
    }

    int current_x = x;
    for (const char *c = text; *c; c++) {
        FallbackFont_RenderChar(current_x, y, *c, color);
        current_x += fallback_font.width;
    }
}

/*
=================
FallbackTexture_Load

Load a fallback texture
=================
*/
qhandle_t FallbackTexture_Load(const char *name) {
    const fallback_asset_t *asset = FS_GetFallbackAsset(name);
    if (!asset || asset->type != FALLBACK_ASSET_TEXTURE) {
        return 0; // Invalid handle
    }

    // This would need integration with the renderer to create actual textures
    // For now, return a dummy handle
    Com_Printf("FallbackTexture: Would load '%s'\n", name);
    return 1; // Dummy handle
}

/*
=================
FallbackTexture_IsAvailable

Check if fallback texture is available
=================
*/
qboolean FallbackTexture_IsAvailable(const char *name) {
    return FS_IsFallbackAssetLoaded(name);
}

/*
=================
FallbackAssets_DisplayErrorMessage

Display an error message using fallback assets
=================
*/
void FallbackAssets_DisplayErrorMessage(const char *title, const char *message) {
    Com_Printf("=== %s ===\n", title);
    Com_Printf("%s\n", message);
    Com_Printf("Using fallback assets for display.\n");

    // In a full implementation, this would render text on screen
    if (FallbackFont_IsAvailable()) {
        FallbackFont_RenderText(10, 10, title, (vec4_t){1, 0, 0, 1});
        FallbackFont_RenderText(10, 30, message, (vec4_t){1, 1, 1, 1});
    }
}

/*
=================
FallbackAssets_DisplayAssetInstructions

Display instructions for obtaining game assets
=================
*/
void FallbackAssets_DisplayAssetInstructions(void) {
    const char *instructions[] = {
        "GAME ASSETS NOT FOUND",
        "",
        "This engine requires game content to display properly.",
        "",
        "Options:",
        "1. Install Quake 3 Arena and copy pak0.pk3-pak8.pk3 to baseq3/",
        "2. Install OpenArena (free) from https://openarena.ws/",
        "",
        "After installing, restart the engine.",
        NULL
    };

    Com_Printf("\n");
    for (int i = 0; instructions[i]; i++) {
        Com_Printf("%s\n", instructions[i]);
    }
    Com_Printf("\n");

    // Display on screen if possible
    if (FallbackFont_IsAvailable()) {
        int y = 50;
        for (int i = 0; instructions[i]; i++) {
            vec4_t color = {1, 1, 1, 1};
            if (i == 0) { // Title in red
                color[0] = 1; color[1] = 0; color[2] = 0;
            }
            FallbackFont_RenderText(10, y, instructions[i], color);
            y += 15;
        }
    }
}