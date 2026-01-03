/*
===========================================================================
tr_font_effects.c - Advanced Font Effects and Rendering Implementation
===========================================================================
*/

#include "tr_font_effects.h"
#include "tr_types.h"
#include "../../common/qcommon.h"

// Forward declarations
static void R_RenderTextBasic(const font_render_context_t *context);

// Font-related cvars - now handled by renderer-specific code

// Font effects state
static qboolean font_effects_initialized = qfalse;
static font_effect_params_t default_effects;

// Font cache for performance
#define FONT_CACHE_SIZE 1024

typedef struct font_cache_entry_s {
    int char_code;
    glyphInfo_t glyph;
} font_cache_entry_t;

static font_cache_entry_t font_glyph_cache[FONT_CACHE_SIZE];
static int font_cache_hits = 0;
static int font_cache_misses = 0;

/*
===============
R_InitFontEffects
===============
*/
void R_InitFontEffects(void) {
    Com_Memset(&default_effects, 0, sizeof(default_effects));
    Com_Memset(font_glyph_cache, 0, sizeof(font_glyph_cache));

    // Set default effect parameters
    R_GetFontEffectDefaults(&default_effects);

    // Initialize font cache
    R_InitFontCache();

    font_effects_initialized = qtrue;
    Com_Printf("Font effects system initialized\n");
}

/*
===============
R_ShutdownFontEffects
===============
*/
void R_ShutdownFontEffects(void) {
    if (!font_effects_initialized) {
        return;
    }

    // Shutdown font cache
    R_ShutdownFontCache();

    font_effects_initialized = qfalse;
    Com_Printf("Font effects system shutdown\n");
}

/*
===============
R_RenderTextWithEffects
===============
*/
void R_RenderTextWithEffects(const font_render_context_t *context) {
    if (!font_effects_initialized || !context || !context->font || !context->text) {
        return;
    }

    font_effect_params_t params = context->effects;

    // Apply effects in correct order: shadow -> outline -> glow -> main text -> animation
    if (params.flags & FONT_EFFECT_SHADOW) {
        R_RenderFontShadow(context);
    }

    if (params.flags & FONT_EFFECT_OUTLINE) {
        R_RenderFontOutline(context);
    }

    if (params.flags & FONT_EFFECT_GLOW) {
        R_RenderFontGlow(context);
    }

    // Render main text
    R_RenderTextBasic(context);

    if (params.flags & FONT_EFFECT_ANIMATION) {
        R_RenderFontAnimation(context);
    }

    if (params.flags & FONT_EFFECT_TRANSFORM) {
        R_RenderFontTransform(context);
    }
}

/*
===============
R_RenderFontGlow
===============
*/
void R_RenderFontGlow(const font_render_context_t *context) {
    if (!context || !(context->effects.flags & FONT_EFFECT_GLOW)) {
        return;
    }

    const font_effect_params_t *params = &context->effects;

    // Create glow effect by rendering text multiple times with decreasing opacity
    vec4_t glow_color = { params->glow_color[0], params->glow_color[1], params->glow_color[2], params->glow_intensity };

    // Render multiple passes for glow effect
    for (int pass = 0; pass < 3; pass++) {
        float offset = (pass + 1) * 2.0f;
        float alpha = params->glow_intensity / (pass + 1);

        glow_color[3] = alpha;

        // Render glow pass (slightly offset and scaled)
        font_render_context_t glow_context = *context;
        glow_context.position[0] += offset * 0.5f;
        glow_context.position[1] += offset * 0.5f;
        glow_context.color[0] = glow_color[0];
        glow_context.color[1] = glow_color[1];
        glow_context.color[2] = glow_color[2];
        glow_context.color[3] = glow_color[3];
        glow_context.scale *= (1.0f + offset * 0.1f);

        R_RenderTextBasic(&glow_context);
    }
}

/*
===============
R_RenderFontOutline
===============
*/
void R_RenderFontOutline(const font_render_context_t *context) {
    if (!context || !(context->effects.flags & FONT_EFFECT_OUTLINE)) {
        return;
    }

    const font_effect_params_t *params = &context->effects;
    vec4_t outline_color = { params->outline_color[0], params->outline_color[1],
                           params->outline_color[2], context->color[3] };

    // Render outline by drawing text in 8 directions around the main text
    static const vec2_t outline_offsets[8] = {
        { -1, -1 }, {  0, -1 }, {  1, -1 },
        { -1,  0 },             {  1,  0 },
        { -1,  1 }, {  0,  1 }, {  1,  1 }
    };

    for (int i = 0; i < 8; i++) {
        font_render_context_t outline_context = *context;
        outline_context.position[0] += outline_offsets[i][0] * params->outline_width;
        outline_context.position[1] += outline_offsets[i][1] * params->outline_width;
        outline_context.color[0] = outline_color[0];
        outline_context.color[1] = outline_color[1];
        outline_context.color[2] = outline_color[2];
        outline_context.color[3] = outline_color[3];

        R_RenderTextBasic(&outline_context);
    }
}

/*
===============
R_RenderFontShadow
===============
*/
void R_RenderFontShadow(const font_render_context_t *context) {
    if (!context || !(context->effects.flags & FONT_EFFECT_SHADOW)) {
        return;
    }

    const font_effect_params_t *params = &context->effects;

    // Render shadow as offset text with blur effect
    font_render_context_t shadow_context = *context;
    shadow_context.position[0] += params->shadow_offset[0];
    shadow_context.position[1] += params->shadow_offset[1];
    shadow_context.color[0] = params->shadow_color[0];
    shadow_context.color[1] = params->shadow_color[1];
    shadow_context.color[2] = params->shadow_color[2];
    shadow_context.color[3] = context->color[3] * 0.7f; // Slightly transparent

    // Apply blur effect if enabled
    if (params->shadow_blur > 0.0f) {
        // Simple blur approximation with multiple passes
        for (int i = 0; i < 3; i++) {
            shadow_context.color[3] *= 0.5f;
            R_RenderTextBasic(&shadow_context);
        }
    } else {
        R_RenderTextBasic(&shadow_context);
    }
}

/*
===============
R_RenderFontAnimation
===============
*/
void R_RenderFontAnimation(const font_render_context_t *context) {
    if (!context || !(context->effects.flags & FONT_EFFECT_ANIMATION)) {
        return;
    }

    const font_effect_params_t *params = &context->effects;
    font_render_context_t anim_context = *context;

    switch (params->animation_type) {
        case FONT_ANIM_PULSE:
            // Pulsing effect
            {
                float pulse = sinf(params->animation_phase * params->animation_speed) * 0.5f + 0.5f;
                anim_context.scale *= (0.8f + pulse * 0.4f);
                anim_context.color[3] *= (0.5f + pulse * 0.5f);
            }
            break;

        case FONT_ANIM_WAVE:
            // Wave effect (would need per-character rendering)
            // For now, apply simple sine wave to entire text
            anim_context.position[1] += sinf(params->animation_phase * params->animation_speed) * 5.0f;
            break;

        case FONT_ANIM_SHAKE:
            // Shake effect
            {
                float shake_x = sinf(params->animation_phase * params->animation_speed * 10.0f) * 2.0f;
                float shake_y = cosf(params->animation_phase * params->animation_speed * 10.0f) * 2.0f;
                anim_context.position[0] += shake_x;
                anim_context.position[1] += shake_y;
            }
            break;

        case FONT_ANIM_FADE:
            // Fade effect
            anim_context.color[3] *= fabsf(sinf(params->animation_phase * params->animation_speed));
            break;

        case FONT_ANIM_RAINBOW:
            // Rainbow color cycling
            {
                float hue = fmodf(params->animation_phase * params->animation_speed, 1.0f);
                // Convert HSV to RGB (simplified rainbow)
                if (hue < 0.33f) {
                    anim_context.color[0] = 1.0f;
                    anim_context.color[1] = hue * 3.0f;
                    anim_context.color[2] = 0.0f;
                } else if (hue < 0.66f) {
                    anim_context.color[0] = (0.66f - hue) * 3.0f;
                    anim_context.color[1] = 1.0f;
                    anim_context.color[2] = 0.0f;
                } else {
                    anim_context.color[0] = 0.0f;
                    anim_context.color[1] = 1.0f;
                    anim_context.color[2] = (hue - 0.66f) * 3.0f;
                }
            }
            break;

        case FONT_ANIM_TYPEWRITER:
            // Typewriter effect (would need text length tracking)
            // For now, simple fade-in effect
            anim_context.color[3] *= fminf(params->animation_phase * params->animation_speed, 1.0f);
            break;

        default:
            break;
    }

    R_RenderTextBasic(&anim_context);
}

/*
===============
R_RenderFontTransform
===============
*/
void R_RenderFontTransform(const font_render_context_t *context) {
    if (!context || !(context->effects.flags & FONT_EFFECT_TRANSFORM)) {
        return;
    }

    // Note: Full transformation would require matrix operations
    // For now, implement basic rotation and scaling
    const font_effect_params_t *params = &context->effects;
    font_render_context_t transform_context = *context;

    // Apply scaling
    transform_context.scale *= params->scale[0]; // Note: This is simplified

    // Rotation would require full matrix transformation
    // This is a placeholder for future implementation

    R_RenderTextBasic(&transform_context);
}

/*
===============
R_RenderTextBasic
===============
*/
static void R_RenderTextBasic(const font_render_context_t *context) {
    // This is a placeholder - the actual text rendering would call
    // the existing RE_RenderString or similar function
    // For now, we'll use the existing font rendering system

    if (context->use_sdf) {
        // Use SDF rendering path
        // Implementation would go here
        // Note: SDF enable/disable is now handled at renderer level
    }

    // Fall back to standard rendering
    // This would call the existing text rendering functions
}

/*
===============
R_GetFontEffectDefaults
===============
*/
void R_GetFontEffectDefaults(font_effect_params_t *params) {
    if (!params) return;

    Com_Memset(params, 0, sizeof(*params));

    // Default glow settings
    VectorSet(params->glow_color, 1.0f, 1.0f, 1.0f);
    params->glow_intensity = 0.5f;
    params->glow_radius = 4.0f;

    // Default outline settings
    VectorSet(params->outline_color, 0.0f, 0.0f, 0.0f);
    params->outline_width = 1.0f;

    // Default shadow settings
    VectorSet(params->shadow_color, 0.0f, 0.0f, 0.0f);
    Vector2Set(params->shadow_offset, 1.0f, 1.0f);
    params->shadow_blur = 0.0f;

    // Default animation settings
    params->animation_type = FONT_ANIM_NONE;
    params->animation_speed = 1.0f;
    params->animation_phase = 0.0f;

    // Default transform settings
    params->rotation_angle = 0.0f;
    Vector2Set(params->scale, 1.0f, 1.0f);
    Vector2Set(params->skew, 0.0f, 0.0f);

    // Default flags
    params->flags = FONT_EFFECT_NONE;
}

/*
===============
R_UpdateFontAnimation
===============
*/
void R_UpdateFontAnimation(font_effect_params_t *params, float delta_time) {
    if (!params || !(params->flags & FONT_EFFECT_ANIMATION)) {
        return;
    }

    params->animation_phase += delta_time * params->animation_speed;

    // Keep phase in reasonable range
    if (params->animation_phase > M_PI * 2.0f) {
        params->animation_phase -= M_PI * 2.0f;
    }
}

/*
===============
R_FontEffectsAvailable
===============
*/
qboolean R_FontEffectsAvailable(void) {
    return font_effects_initialized;
}

/*
===============
R_GetTextBounds
===============
*/
void R_GetTextBounds(const fontInfo_t *font, const char *text, int text_length,
                    float scale, vec2_t mins, vec2_t maxs) {
    if (!font || !text || text_length <= 0) {
        mins[0] = mins[1] = 0;
        maxs[0] = maxs[1] = 0;
        return;
    }

    // Implementation would calculate text bounds
    // For now, provide basic implementation
    float width = text_length * font->glyphScale * scale * 8.0f; // Rough estimate
    float height = font->glyphScale * scale * 16.0f; // Rough estimate

    mins[0] = 0;
    mins[1] = -height;
    maxs[0] = width;
    maxs[1] = 0;
}

/*
===============
R_GetTextAdvance
===============
*/
void R_GetTextAdvance(const fontInfo_t *font, const char *text, int text_length,
                     float scale, vec2_t advance) {
    if (!font || !text) {
        advance[0] = advance[1] = 0;
        return;
    }

    // Implementation would calculate text advance
    // For now, provide basic implementation
    advance[0] = text_length * font->glyphScale * scale * 8.0f; // Rough estimate
    advance[1] = 0;
}

/*
===============
R_IsUnicodeSupported
===============
*/
qboolean R_IsUnicodeSupported(void) {
    return qtrue; // Assume Unicode support is available
}

/*
===============
R_UTF8ToUnicode
===============
*/
int R_UTF8ToUnicode(const char *utf8, int *unicode) {
    if (!utf8 || !unicode) return 0;

    // Basic UTF-8 to Unicode conversion (simplified)
    unsigned char c = (unsigned char)*utf8;
    if (c < 0x80) {
        *unicode = c;
        return 1;
    } else if ((c & 0xE0) == 0xC0) {
        // 2-byte sequence
        *unicode = ((c & 0x1F) << 6) | ((unsigned char)utf8[1] & 0x3F);
        return 2;
    }

    // More complex cases would be handled here
    *unicode = '?'; // Fallback
    return 1;
}

/*
===============
R_UnicodeToUTF8
===============
*/
int R_UnicodeToUTF8(int unicode, char *utf8, int max_bytes) {
    if (!utf8 || max_bytes < 1) return 0;

    if (unicode < 0x80 && max_bytes >= 1) {
        utf8[0] = (char)unicode;
        return 1;
    } else if (unicode < 0x800 && max_bytes >= 2) {
        utf8[0] = (char)(0xC0 | (unicode >> 6));
        utf8[1] = (char)(0x80 | (unicode & 0x3F));
        return 2;
    }

    // More complex cases would be handled here
    utf8[0] = '?'; // Fallback
    return 1;
}

/*
===============
R_GetFallbackFont
===============
*/
fontInfo_t *R_GetFallbackFont(const fontInfo_t *primary_font, int char_code) {
    // Implementation would find appropriate fallback font based on char_code
    // For now, return the primary font
    (void)char_code; // Suppress unused parameter warning
    return (fontInfo_t *)primary_font;
}

/*
===============
R_LoadLanguageFonts
===============
*/
qboolean R_LoadLanguageFonts(const char *language_code) {
    // Implementation would load language-specific fonts
    // For now, return success
    (void)language_code; // Suppress unused parameter warning
    return qtrue;
}

/*
===============
R_InitFontCache
===============
*/
void R_InitFontCache(void) {
    Com_Memset(font_glyph_cache, 0, sizeof(font_glyph_cache));
    font_cache_hits = 0;
    font_cache_misses = 0;
}

/*
===============
R_ShutdownFontCache
===============
*/
void R_ShutdownFontCache(void) {
    // Clean up cached glyphs if needed
    Com_Memset(font_glyph_cache, 0, sizeof(font_glyph_cache));
}

/*
===============
R_GetCachedGlyph
===============
*/
glyphInfo_t *R_GetCachedGlyph(fontInfo_t *font, int char_code) {
    // Simple hash-based cache lookup
    unsigned int hash = (unsigned int)char_code % FONT_CACHE_SIZE;

    (void)font; // Suppress unused parameter warning - font context not needed for simple cache

    font_cache_entry_t *cached = &font_glyph_cache[hash];
    if (cached->char_code == char_code && cached->glyph.glyph != 0) {
        font_cache_hits++;
        return &cached->glyph;
    }

    font_cache_misses++;
    return NULL;
}

/*
===============
R_StreamFontGlyphs
===============
*/
void R_StreamFontGlyphs(fontInfo_t *font, const char *text) {
    // Implementation would stream glyphs on demand
    // For now, this is a placeholder
    (void)font;   // Suppress unused parameter warning
    (void)text;   // Suppress unused parameter warning
}

/*
===============
R_GetFontStats
===============
*/
void R_GetFontStats(int *cached_glyphs, int *total_glyphs, int *cache_hits, int *cache_misses) {
    if (cached_glyphs) *cached_glyphs = 0; // Would count actual cached glyphs
    if (total_glyphs) *total_glyphs = 0;   // Would count total requested glyphs
    if (cache_hits) *cache_hits = font_cache_hits;
    if (cache_misses) *cache_misses = font_cache_misses;
}
