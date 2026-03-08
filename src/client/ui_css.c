/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

UI CSS styling subsystem - minimal CSS parser and style registry.
Supports: #id, .class, element selectors; color, font-size, font-family,
background-color, border, border-color. No external dependencies.
===========================================================================
*/

#include "client.h"
#include "ui_css.h"

#define UICSS_SELECTOR_LEN  64
#define UICSS_VALUE_LEN    128

typedef enum {
	UICSS_SEL_ID,
	UICSS_SEL_CLASS,
	UICSS_SEL_TAG
} ui_css_selector_type_t;

typedef struct {
	ui_css_selector_type_t type;
	char value[UICSS_SELECTOR_LEN];
	vec4_t color;
	vec4_t background_color;
	vec4_t border_color;
	float border;
	float font_size;
	char font[UICSS_FONT_LEN];
	qboolean has_color;
	qboolean has_background_color;
	qboolean has_border_color;
	qboolean has_border;
	qboolean has_font_size;
	qboolean has_font;
} ui_css_rule_t;

static ui_css_rule_t css_rules[UICSS_MAX_RULES];
static int css_num_rules;
static qboolean css_initialized;
static cvar_t *ui_stylesheet;

static void skip_whitespace( const char **pp ) {
	const char *p = *pp;
	while ( *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' ) p++;
	*pp = p;
}

static void skip_comment( const char **pp ) {
	const char *p = *pp;
	if ( p[0] == '/' && p[1] == '*' ) {
		p += 2;
		while ( p[0] && !( p[0] == '*' && p[1] == '/' ) ) p++;
		if ( p[0] ) p += 2;
		*pp = p;
	}
}

static void skip_ws_and_comments( const char **pp ) {
	for ( ;; ) {
		skip_whitespace( pp );
		if ( (*pp)[0] == '/' && (*pp)[1] == '*' ) {
			skip_comment( pp );
		} else {
			break;
		}
	}
}

/* Parse #hex, #rgb, rgb(r,g,b), rgba(r,g,b,a) into vec4_t. Returns qfalse on parse error. */
static qboolean parse_color( const char *s, vec4_t out ) {
	int r, g, b, a;
	float fr, fg, fb, fa;
	if ( !s || !s[0] ) return qfalse;
	out[0] = out[1] = out[2] = 1.0f;
	out[3] = 1.0f;

	if ( s[0] == '#' ) {
		s++;
		if ( strlen( s ) == 6 ) {
			if ( sscanf( s, "%2x%2x%2x", &r, &g, &b ) == 3 ) {
				out[0] = r / 255.0f;
				out[1] = g / 255.0f;
				out[2] = b / 255.0f;
				return qtrue;
			}
		} else if ( strlen( s ) == 3 ) {
			if ( sscanf( s, "%1x%1x%1x", &r, &g, &b ) == 3 ) {
				out[0] = ( r * 17 ) / 255.0f;
				out[1] = ( g * 17 ) / 255.0f;
				out[2] = ( b * 17 ) / 255.0f;
				return qtrue;
			}
		}
		return qfalse;
	}
	if ( Q_stricmpn( s, "rgba(", 5 ) == 0 ) {
		if ( sscanf( s + 5, "%f,%f,%f,%f", &fr, &fg, &fb, &fa ) == 4 ) {
			out[0] = Com_Clamp( 0.0f, 1.0f, fr );
			out[1] = Com_Clamp( 0.0f, 1.0f, fg );
			out[2] = Com_Clamp( 0.0f, 1.0f, fb );
			out[3] = Com_Clamp( 0.0f, 1.0f, fa );
			return qtrue;
		}
	}
	if ( Q_stricmpn( s, "rgb(", 4 ) == 0 ) {
		if ( sscanf( s + 4, "%d,%d,%d", &r, &g, &b ) == 3 ) {
			out[0] = Com_Clamp( 0, 255, r ) / 255.0f;
			out[1] = Com_Clamp( 0, 255, g ) / 255.0f;
			out[2] = Com_Clamp( 0, 255, b ) / 255.0f;
			return qtrue;
		}
		if ( sscanf( s + 4, "%f,%f,%f", &fr, &fg, &fb ) == 3 ) {
			out[0] = Com_Clamp( 0.0f, 1.0f, fr );
			out[1] = Com_Clamp( 0.0f, 1.0f, fg );
			out[2] = Com_Clamp( 0.0f, 1.0f, fb );
			return qtrue;
		}
	}
	return qfalse;
}

/* Copy token into buf, advance p past token. Token ends at space, ;, }, :, etc. */
static void read_token( const char **pp, char *buf, int maxlen ) {
	const char *p = *pp;
	int i = 0;
	while ( *p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r' &&
		*p != ';' && *p != '}' && *p != ':' && *p != '{' && i < maxlen - 1 ) {
		if ( *p == '"' ) {
			p++;
			while ( *p && *p != '"' && i < maxlen - 1 ) buf[i++] = *p++;
			if ( *p == '"' ) p++;
			break;
		}
		buf[i++] = *p++;
	}
	buf[i] = '\0';
	*pp = p;
}

/* Read value (may include spaces until ;) */
static void read_value( const char **pp, char *buf, int maxlen ) {
	const char *p = *pp;
	int i = 0;
	skip_ws_and_comments( pp );
	p = *pp;
	while ( *p && *p != ';' && *p != '}' && i < maxlen - 1 ) {
		if ( *p == '"' ) {
			p++;
			while ( *p && *p != '"' && i < maxlen - 1 ) buf[i++] = *p++;
			if ( *p == '"' ) p++;
			continue;
		}
		buf[i++] = *p++;
	}
	while ( i > 0 && ( buf[i-1] == ' ' || buf[i-1] == '\t' ) ) i--;
	buf[i] = '\0';
	*pp = p;
}

static qboolean parse_rule( const char **pp, ui_css_rule_t *rule ) {
	char tok[UICSS_VALUE_LEN];
	const char *p = *pp;

	Com_Memset( rule, 0, sizeof( *rule ) );
	rule->color[0] = rule->color[1] = rule->color[2] = rule->color[3] = 1.0f;
	rule->background_color[3] = 0.0f;
	rule->border_color[3] = 0.0f;

	read_token( &p, tok, sizeof( tok ) );
	if ( !tok[0] ) return qfalse;

	if ( tok[0] == '#' ) {
		rule->type = UICSS_SEL_ID;
		Q_strncpyz( rule->value, tok + 1, sizeof( rule->value ) );
	} else if ( tok[0] == '.' ) {
		rule->type = UICSS_SEL_CLASS;
		Q_strncpyz( rule->value, tok + 1, sizeof( rule->value ) );
	} else {
		rule->type = UICSS_SEL_TAG;
		Q_strncpyz( rule->value, tok, sizeof( rule->value ) );
	}

	skip_ws_and_comments( &p );
	if ( *p != '{' ) return qfalse;
	p++;

	while ( 1 ) {
		char prop[64];
		char val[UICSS_VALUE_LEN];
		skip_ws_and_comments( &p );
		if ( *p == '}' || !*p ) break;
		read_token( &p, prop, sizeof( prop ) );
		if ( !prop[0] ) break;
		skip_ws_and_comments( &p );
		if ( *p != ':' ) continue;
		p++;
		read_value( &p, val, sizeof( val ) );
		if ( Q_stricmp( prop, "color" ) == 0 ) {
			if ( parse_color( val, rule->color ) ) rule->has_color = qtrue;
		} else if ( Q_stricmp( prop, "background-color" ) == 0 || Q_stricmp( prop, "background" ) == 0 ) {
			if ( parse_color( val, rule->background_color ) ) {
				rule->has_background_color = qtrue;
				if ( rule->background_color[3] <= 0.0f ) rule->background_color[3] = 0.5f;
			}
		} else if ( Q_stricmp( prop, "border-color" ) == 0 ) {
			if ( parse_color( val, rule->border_color ) ) rule->has_border_color = qtrue;
		} else if ( Q_stricmp( prop, "border" ) == 0 ) {
			float b = (float)atof( val );
			if ( b >= 0 ) { rule->border = b; rule->has_border = qtrue; }
		} else if ( Q_stricmp( prop, "font-size" ) == 0 ) {
			float sz = (float)atof( val );
			if ( sz > 0 ) { rule->font_size = sz; rule->has_font_size = qtrue; }
		} else if ( Q_stricmp( prop, "font-family" ) == 0 || Q_stricmp( prop, "font" ) == 0 ) {
			Q_strncpyz( rule->font, val, sizeof( rule->font ) );
			rule->has_font = qtrue;
		}
		skip_ws_and_comments( &p );
		if ( *p == ';' ) p++;
	}
	if ( *p == '}' ) p++;
	*pp = p;
	return qtrue;
}

void UICSS_Init( void ) {
	Com_Memset( css_rules, 0, sizeof( css_rules ) );
	css_num_rules = 0;
	css_initialized = qtrue;
	ui_stylesheet = Cvar_Get( "ui_stylesheet", "", CVAR_ARCHIVE );
	Cvar_SetDescription( ui_stylesheet, "CSS stylesheet for UI styling (e.g. ui/default.css). Supports #id, .class, element selectors." );
	if ( ui_stylesheet->string[0] ) {
		UICSS_LoadStylesheet( ui_stylesheet->string );
	}
	Com_Printf( "UICSS: initialized (ui_stylesheet=%s)\n",
		ui_stylesheet->string[0] ? ui_stylesheet->string : "(none)" );
}

void UICSS_Shutdown( void ) {
	Com_Memset( css_rules, 0, sizeof( css_rules ) );
	css_num_rules = 0;
	css_initialized = qfalse;
}

void UICSS_LoadStylesheet( const char *path ) {
	void *buf;
	int len;
	const char *p;
	ui_css_rule_t rule;

	if ( !path || !path[0] ) return;
	if ( !css_initialized ) return;

	len = FS_ReadFile( path, &buf );
	if ( len <= 0 || !buf ) {
		Com_Printf( S_COLOR_YELLOW "UICSS: could not load %s\n", path );
		css_num_rules = 0;
		return;
	}

	css_num_rules = 0;
	p = (const char *)buf;

	while ( css_num_rules < UICSS_MAX_RULES ) {
		skip_ws_and_comments( &p );
		if ( !*p ) break;
		if ( parse_rule( &p, &rule ) ) {
			css_rules[css_num_rules++] = rule;
		} else {
			/* skip to next } or end */
			while ( *p && *p != '}' ) p++;
			if ( *p == '}' ) p++;
		}
	}

	FS_FreeFile( buf );
	Com_Printf( "UICSS: loaded %s (%d rules)\n", path, css_num_rules );
}

static qboolean selector_matches( const ui_css_rule_t *r, const char *id, const char *class_name, const char *tag ) {
	switch ( r->type ) {
		case UICSS_SEL_ID:
			return id && Q_stricmp( r->value, id ) == 0;
		case UICSS_SEL_CLASS:
			return class_name && Q_stricmp( r->value, class_name ) == 0;
		case UICSS_SEL_TAG:
			return tag && Q_stricmp( r->value, tag ) == 0;
	}
	return qfalse;
}

qboolean UICSS_GetStyles( const char *id, const char *class_name, const char *tag, ui_css_styles_t *out ) {
	int i;
	qboolean any = qfalse;

	if ( !out || !css_initialized ) return qfalse;
	Com_Memset( out, 0, sizeof( *out ) );
	out->color[0] = out->color[1] = out->color[2] = out->color[3] = 1.0f;

	for ( i = 0; i < css_num_rules; i++ ) {
		const ui_css_rule_t *r = &css_rules[i];
		if ( !selector_matches( r, id, class_name, tag ) ) continue;
		any = qtrue;
		if ( r->has_color ) {
			Vector4Copy( r->color, out->color );
			out->has_color = qtrue;
		}
		if ( r->has_background_color ) {
			Vector4Copy( r->background_color, out->background_color );
			out->has_background_color = qtrue;
		}
		if ( r->has_border_color ) {
			Vector4Copy( r->border_color, out->border_color );
			out->has_border_color = qtrue;
		}
		if ( r->has_border ) {
			out->border = r->border;
			out->has_border = qtrue;
		}
		if ( r->has_font_size ) {
			out->font_size = r->font_size;
			out->has_font_size = qtrue;
		}
		if ( r->has_font ) {
			Q_strncpyz( out->font, r->font, sizeof( out->font ) );
			out->has_font = qtrue;
		}
	}
	return any;
}

void UICSS_ApplyToVec4( const ui_css_styles_t *css, vec4_t color, int which ) {
	if ( !css ) return;
	switch ( which ) {
		case UICSS_APPLY_COLOR:
			if ( css->has_color ) Vector4Copy( css->color, color );
			break;
		case UICSS_APPLY_BGCOLOR:
			if ( css->has_background_color ) Vector4Copy( css->background_color, color );
			break;
		case UICSS_APPLY_BORDERCOLOR:
			if ( css->has_border_color ) Vector4Copy( css->border_color, color );
			break;
	}
}

float UICSS_ApplyFontSize( const ui_css_styles_t *css, float default_size ) {
	if ( !css || !css->has_font_size ) return default_size;
	return css->font_size;
}

void UICSS_ApplyFont( const ui_css_styles_t *css, char *out, int maxlen, const char *default_font ) {
	if ( !out || maxlen <= 0 ) return;
	if ( css && css->has_font && css->font[0] ) {
		Q_strncpyz( out, css->font, maxlen );
	} else if ( default_font ) {
		Q_strncpyz( out, default_font, maxlen );
	} else {
		out[0] = '\0';
	}
}
