/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

CSS-style UI filter / backdrop-filter blur (client side). See ui_filter.h.
===========================================================================
*/

#include "client.h"
#include "ui_filter.h"

static cvar_t *cl_uiFilter;
static qboolean uifilter_initialized;

/*
===============
UIFilter_Init
===============
*/
void UIFilter_Init( void ) {
	cl_uiFilter = Cvar_Get( "cl_uiFilter", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cl_uiFilter,
		"CSS-style UI filter/backdrop-filter blur: 0 = plain translucent fallback, 1 = renderer blur when available" );
	uifilter_initialized = qtrue;
	Com_Printf( "UIFilter: CSS filter/backdrop-filter blur %s (renderer %s)\n",
		cl_uiFilter->integer ? "enabled" : "disabled",
		( re.UIBackdropBlur && re.UIFilterLayer ) ? "supported" : "unsupported" );
}

/*
===============
UIFilter_Available
===============
*/
qboolean UIFilter_Available( void ) {
	if ( !uifilter_initialized || !cl_uiFilter || !cl_uiFilter->integer ) {
		return qfalse;
	}
	if ( !re.UIBackdropBlur || !re.UIFilterLayer ) {
		return qfalse;
	}
	if ( Cvar_VariableIntegerValue( "ui_blurQuality" ) <= 0 ) {
		return qfalse;
	}
	/* Renderer publishes readiness after init / swapchain restore. Missing or
	 * 0 means compositor is down — fall back to plain translucent panels. */
	if ( Cvar_VariableIntegerValue( "ui_blurReady" ) <= 0 ) {
		return qfalse;
	}
	return qtrue;
}

/*
===============
UIFilter_NormalizedRect

Convert a 640x480 virtual rect into normalized [0,1] screen space using the
same aspect/ui_scale mapping as SCR_AdjustFrom640.
===============
*/
static void UIFilter_NormalizedRect( float x, float y, float w, float h, float rect[4] ) {
	float ax = x, ay = y, aw = w, ah = h;

	SCR_AdjustFrom640( &ax, &ay, &aw, &ah );
	if ( cls.glconfig.vidWidth <= 0 || cls.glconfig.vidHeight <= 0 ) {
		rect[0] = rect[1] = 0.0f;
		rect[2] = rect[3] = 1.0f;
		return;
	}
	rect[0] = ax / (float)cls.glconfig.vidWidth;
	rect[1] = ay / (float)cls.glconfig.vidHeight;
	rect[2] = ( ax + aw ) / (float)cls.glconfig.vidWidth;
	rect[3] = ( ay + ah ) / (float)cls.glconfig.vidHeight;
}

/* Corner radius normalized to min(rect width, rect height) in virtual px. */
static float UIFilter_NormalizedCorner( float cornerRadius, float w, float h ) {
	float minWH = ( w < h ) ? w : h;
	if ( cornerRadius <= 0.0f || minWH <= 0.0f ) {
		return 0.0f;
	}
	return Com_Clamp( 0.0f, 0.5f, cornerRadius / minWH );
}

/*
===============
SCR_UIBackdropBlur
===============
*/
void SCR_UIBackdropBlur( float x, float y, float w, float h, float radius,
	float cornerRadius, float rotation, float opacity, const vec4_t tint )
{
	uiBackdropFilter_t bf;

	if ( w <= 0.0f || h <= 0.0f ) {
		return;
	}
	if ( opacity <= 0.0f ) {
		return;
	}

	if ( !UIFilter_Available() || radius <= 0.0f ) {
		/* graceful fallback: plain translucent panel, no blur. */
		vec4_t fill;
		if ( tint && tint[3] > 0.0f ) {
			Vector4Copy( tint, fill );
			fill[3] *= opacity;
		} else {
			fill[0] = fill[1] = fill[2] = 0.1f;
			fill[3] = 0.5f * opacity;
		}
		SCR_FillRect( x, y, w, h, fill );
		return;
	}

	Com_Memset( &bf, 0, sizeof( bf ) );
	UIFilter_NormalizedRect( x, y, w, h, bf.rect );
	bf.cornerRadius = UIFilter_NormalizedCorner( cornerRadius, w, h );
	bf.radius = radius;
	bf.rotation = rotation;
	bf.opacity = Com_Clamp( 0.0f, 1.0f, opacity );
	if ( tint ) {
		Vector4Copy( tint, bf.tint );
	}
	re.UIBackdropBlur( &bf );
}

/*
===============
SCR_UIFilterLayer
===============
*/
void SCR_UIFilterLayer( float x, float y, float w, float h, qhandle_t hShader,
	float radius, float cornerRadius, float rotation, float opacity )
{
	uiCompositorLayer_t layer;

	if ( w <= 0.0f || h <= 0.0f || !hShader ) {
		return;
	}
	if ( opacity <= 0.0f ) {
		return;
	}

	if ( !UIFilter_Available() || radius <= 0.0f ) {
		/* graceful fallback: draw the element unblurred. */
		SCR_DrawPic( x, y, w, h, hShader );
		return;
	}

	Com_Memset( &layer, 0, sizeof( layer ) );
	UIFilter_NormalizedRect( x, y, w, h, layer.rect );
	layer.cornerRadius = UIFilter_NormalizedCorner( cornerRadius, w, h );
	layer.rotation = rotation;
	layer.opacity = Com_Clamp( 0.0f, 1.0f, opacity );
	layer.shader = hShader;
	layer.filter.numOps = 1;
	layer.filter.ops[0].type = UI_FILTER_BLUR;
	layer.filter.ops[0].radius = radius;
	re.UIFilterLayer( &layer );
}

/*
===============
UIFilter_ParseChain

Parse CSS filter values: "blur(8px)", "blur(1.5em)" (em treated as 16px),
optionally space-separated chains. Unknown functions are skipped.
===============
*/
qboolean UIFilter_ParseChain( const char *value, uiFilterChain_t *out ) {
	const char *p = value;

	if ( !out ) {
		return qfalse;
	}
	Com_Memset( out, 0, sizeof( *out ) );
	if ( !p || !p[0] ) {
		return qfalse;
	}

	while ( *p && out->numOps < UI_MAX_FILTER_OPS ) {
		while ( *p == ' ' || *p == '\t' ) p++;
		if ( !*p ) {
			break;
		}
		if ( Q_stricmpn( p, "blur(", 5 ) == 0 ) {
			float r = (float)atof( p + 5 );
			const char *unit = p + 5;
			while ( *unit && ( ( *unit >= '0' && *unit <= '9' ) || *unit == '.' || *unit == '-' || *unit == '+' ) ) unit++;
			if ( Q_stricmpn( unit, "em", 2 ) == 0 ) {
				r *= 16.0f;
			}
			if ( r > 0.0f ) {
				out->ops[out->numOps].type = UI_FILTER_BLUR;
				out->ops[out->numOps].radius = r;
				out->numOps++;
			}
		}
		/* skip past this function's closing paren (or bare token). */
		while ( *p && *p != ')' && *p != ' ' ) p++;
		if ( *p == ')' ) p++;
	}
	return (qboolean)( out->numOps > 0 );
}
