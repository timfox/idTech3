/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

UI CSS styling subsystem.

Loads .css files to control UI appearance: fonts, colors, sizes, effects.
Supports a subset of CSS: #id, .class, element selectors and common
properties. Used by SuperHUD and other UI systems.
===========================================================================
*/

#ifndef UI_CSS_H
#define UI_CSS_H

#include "q_shared.h"

#define UICSS_MAX_RULES     256
#define UICSS_FONT_LEN      64

/* Computed styles for a UI element. Values are applied when non-default. */
typedef struct {
	vec4_t  color;           /* text/foreground color (RGBA 0-1) */
	vec4_t  background_color;
	vec4_t  border_color;
	float   border;
	float   font_size;
	char    font[UICSS_FONT_LEN];
	float   filter_blur;     /* filter: blur(Npx) radius, virtual px */
	float   backdrop_blur;   /* backdrop-filter / -webkit-backdrop-filter: blur(Npx) */
	float   border_radius;   /* border-radius in virtual px */
	qboolean has_color;
	qboolean has_background_color;
	qboolean has_border_color;
	qboolean has_border;
	qboolean has_font_size;
	qboolean has_font;
	qboolean has_filter_blur;
	qboolean has_backdrop_blur;
	qboolean has_border_radius;
} ui_css_styles_t;

void     UICSS_Init( void );
void     UICSS_Shutdown( void );
void     UICSS_LoadStylesheet( const char *path );
qboolean UICSS_GetStyles( const char *id, const char *class_name, const char *tag,
	ui_css_styles_t *out );
void     UICSS_ApplyToVec4( const ui_css_styles_t *css, vec4_t color, int which );
float    UICSS_ApplyFontSize( const ui_css_styles_t *css, float default_size );
void     UICSS_ApplyFont( const ui_css_styles_t *css, char *out, int maxlen, const char *default_font );

/* which: 0=color, 1=background_color, 2=border_color */
#define UICSS_APPLY_COLOR           0
#define UICSS_APPLY_BGCOLOR         1
#define UICSS_APPLY_BORDERCOLOR     2

#endif /* UI_CSS_H */
