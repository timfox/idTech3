/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

CSS-style UI filter / backdrop-filter blur (client side).

Thin wrapper around the renderer's UI blur compositor (renderers/vulkan/
vk_ui_blur.c). Converts 640x480 virtual UI coordinates into the normalized
screen-space rects the renderer consumes, parses CSS "blur(Npx)" values, and
degrades to a plain translucent fill when the renderer path is unavailable.
===========================================================================
*/

#ifndef UI_FILTER_H
#define UI_FILTER_H

#include "q_shared.h"
#include "../renderers/common/tr_types.h"

void UIFilter_Init( void );

/* True when the renderer exports the blur compositor and ui_blurQuality > 0. */
qboolean UIFilter_Available( void );

/*
 * Backdrop blur panel (CSS backdrop-filter: blur(radius)).
 * Coordinates are 640x480 virtual values; radius in virtual pixels.
 * cornerRadius in virtual pixels (0 = square), rotation in radians about the
 * rect center, opacity 0..1, tint = optional straight-alpha RGBA overlay
 * (NULL or tint[3] <= 0 for none). Falls back to a translucent fill when the
 * blur path is unavailable.
 */
void SCR_UIBackdropBlur( float x, float y, float w, float h, float radius,
	float cornerRadius, float rotation, float opacity, const vec4_t tint );

/* Native render-pixel variant for surfaces that intentionally span the full
 * widescreen framebuffer (for example the developer console). Returns qtrue
 * when the renderer compositor accepted the blur operation. */
qboolean SCR_UIBackdropBlurScreen( float x, float y, float w, float h, float radius,
	float cornerRadius, float rotation, float opacity, const vec4_t tint );

/*
 * Filtered layer (CSS filter: blur(radius)) drawing a single shader/image.
 * Same coordinate conventions as SCR_UIBackdropBlur. Falls back to a plain
 * SCR_DrawPic when the blur path is unavailable.
 */
void SCR_UIFilterLayer( float x, float y, float w, float h, qhandle_t hShader,
	float radius, float cornerRadius, float rotation, float opacity );

/*
 * Parse a CSS filter value like "blur(8px)" (optionally a space-separated
 * chain) into a uiFilterChain_t. Returns qtrue when at least one op parsed.
 */
qboolean UIFilter_ParseChain( const char *value, uiFilterChain_t *out );

#endif /* UI_FILTER_H */
