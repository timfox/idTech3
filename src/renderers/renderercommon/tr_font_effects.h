/*
===========================================================================
tr_font_effects.h - Advanced Font Effects and Rendering
===========================================================================
*/

#ifndef __TR_FONT_EFFECTS_H__
#define __TR_FONT_EFFECTS_H__

#include "tr_types.h"

// Font effect types
typedef enum {
    FONT_EFFECT_NONE = 0,
    FONT_EFFECT_GLOW = (1 << 0),
    FONT_EFFECT_OUTLINE = (1 << 1),
    FONT_EFFECT_SHADOW = (1 << 2),
    FONT_EFFECT_ANIMATION = (1 << 3),
    FONT_EFFECT_TRANSFORM = (1 << 4),
    FONT_EFFECT_ALL = 0xFFFFFFFF
} font_effect_flags_t;

// Font animation types
typedef enum {
    FONT_ANIM_NONE,
    FONT_ANIM_PULSE,
    FONT_ANIM_WAVE,
    FONT_ANIM_SHAKE,
    FONT_ANIM_FADE,
    FONT_ANIM_RAINBOW,
    FONT_ANIM_TYPEWRITER
} font_animation_type_t;

// Font effect parameters
typedef struct {
    // Glow effect
    vec3_t glow_color;
    float glow_intensity;
    float glow_radius;

    // Outline effect
    vec3_t outline_color;
    float outline_width;

    // Shadow effect
    vec3_t shadow_color;
    vec2_t shadow_offset;
    float shadow_blur;

    // Animation effect
    font_animation_type_t animation_type;
    float animation_speed;
    float animation_phase;

    // Transform effect
    float rotation_angle;  // degrees
    vec2_t scale;         // x,y scaling factors
    vec2_t skew;          // x,y skew factors

    // Effect flags
    font_effect_flags_t flags;
} font_effect_params_t;

// Font rendering context
typedef struct {
    fontInfo_t *font;
    const char *text;
    int text_length;
    vec2_t position;
    vec4_t color;
    float scale;
    font_effect_params_t effects;
    qboolean use_sdf;     // Use signed distance field rendering
} font_render_context_t;

// Function declarations
void R_InitFontEffects(void);
void R_ShutdownFontEffects(void);

// Main font rendering with effects
void R_RenderTextWithEffects(const font_render_context_t *context);

// Individual effect functions
void R_RenderFontGlow(const font_render_context_t *context);
void R_RenderFontOutline(const font_render_context_t *context);
void R_RenderFontShadow(const font_render_context_t *context);
void R_RenderFontAnimation(const font_render_context_t *context);
void R_RenderFontTransform(const font_render_context_t *context);

// Utility functions
void R_GetFontEffectDefaults(font_effect_params_t *params);
qboolean R_FontEffectsAvailable(void);
void R_UpdateFontAnimation(font_effect_params_t *params, float delta_time);

// Font metrics and layout
void R_GetTextBounds(const fontInfo_t *font, const char *text, int text_length,
                    float scale, vec2_t mins, vec2_t maxs);
void R_GetTextAdvance(const fontInfo_t *font, const char *text, int text_length,
                     float scale, vec2_t advance);

// Unicode and internationalization
qboolean R_IsUnicodeSupported(void);
int R_UTF8ToUnicode(const char *utf8, int *unicode);
int R_UnicodeToUTF8(int unicode, char *utf8, int max_bytes);

// Font fallback and multilingual support
fontInfo_t *R_GetFallbackFont(const fontInfo_t *primary_font, int char_code);
qboolean R_LoadLanguageFonts(const char *language_code);

// Font caching and streaming
void R_InitFontCache(void);
void R_ShutdownFontCache(void);
glyphInfo_t *R_GetCachedGlyph(fontInfo_t *font, int char_code);
void R_StreamFontGlyphs(fontInfo_t *font, const char *text);

// Performance monitoring
void R_GetFontStats(int *cached_glyphs, int *total_glyphs, int *cache_hits, int *cache_misses);

#endif // __TR_FONT_EFFECTS_H__
