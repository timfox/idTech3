/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

SuperHUD configurable HUD layout system.
Parses .cfg-based HUD element definitions and renders them.
Compatible with OSP2-BE SuperHUD config format.
===========================================================================
*/

#ifndef CL_SUPERHUD_H
#define CL_SUPERHUD_H

#include "../qcommon/q_shared.h"

#define SHUD_MAX_ELEMENTS   128
#define SHUD_MAX_CONFIGS    8

typedef enum {
	SHUD_TEXT,
	SHUD_BAR,
	SHUD_ICON,
	SHUD_NUMBER,
	SHUD_TIMER,
	SHUD_RECT,
	SHUD_TYPE_COUNT
} shudElementType_t;

typedef enum {
	SHUD_ANCHOR_TL, SHUD_ANCHOR_TC, SHUD_ANCHOR_TR,
	SHUD_ANCHOR_ML, SHUD_ANCHOR_MC, SHUD_ANCHOR_MR,
	SHUD_ANCHOR_BL, SHUD_ANCHOR_BC, SHUD_ANCHOR_BR
} shudAnchor_t;

typedef struct {
	char                name[64];
	shudElementType_t   type;
	shudAnchor_t        anchor;
	float               x, y, w, h;
	vec4_t              color;
	vec4_t              bgcolor;
	vec4_t              bordercolor;
	float               border;
	char                text[128];
	char                font[64];
	float               fontsize;
	int                 style;
	int                 visflags;
	qboolean            active;
} shudElement_t;

typedef struct {
	char            name[64];
	shudElement_t   elements[SHUD_MAX_ELEMENTS];
	int             numElements;
	qboolean        loaded;
} shudConfig_t;

void    SHUD_Init( void );
void    SHUD_Shutdown( void );
void    SHUD_LoadConfig( const char *filename );
void    SHUD_Render( int screenW, int screenH );
int     SHUD_AddElement( const char *name, shudElementType_t type );
void    SHUD_SetElementPos( int idx, float x, float y, float w, float h );
void    SHUD_SetElementColor( int idx, const vec4_t color );
void    SHUD_SetElementText( int idx, const char *text );

#endif /* CL_SUPERHUD_H */
