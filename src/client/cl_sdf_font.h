/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Signed Distance Field (SDF) font rendering.
Renders resolution-independent text that stays sharp at any scale.
Supports outlines, drop shadows, and glow effects via the SDF shader.

SDF fonts are generated offline from TTF/OTF files using tools like:
  - msdfgen (multi-channel SDF)
  - Hiero (libGDX font tool)
  - fontbm (BMFont with SDF support)

The atlas texture stores the signed distance to the glyph edge in
the alpha channel (0.5 = glyph edge). The default 2D path uses linear
filtering on that field; dedicated `sdf_text.frag` exists for a future
GPU smoothstep path.

`SDF_DrawStringExt` takes a coordinate space: **virtual 640×480** for
big HUD strings (`SCR_DrawStringExt`) and **native screen pixels** for
the console (`SCR_DrawSmallString*`).
===========================================================================
*/

#ifndef CL_SDF_FONT_H
#define CL_SDF_FONT_H

#include "../qcommon/q_shared.h"

#define SDF_MAX_FONTS       8
#define SDF_MAX_GLYPHS      4096
#define SDF_MAX_KERNINGS    8192
#define SDF_INVALID_HANDLE  (-1)

typedef int sdfFontHandle_t;

typedef struct {
	int     id;
	float   x, y, w, h;
	float   xoffset, yoffset;
	float   xadvance;
	float   s0, t0, s1, t1;
} sdfGlyph_t;

typedef struct {
	uint32_t first;
	uint32_t second;
	float    amount;
} sdfKerning_t;

typedef struct {
	float   outlineWidth;
	float   outlineColor[4];
	float   shadowOffset[2];
	float   shadowSoftness;
	float   shadowColor[3];
	float   smoothing;
} sdfDrawParams_t;

void            SDF_Init( void );
void            SDF_Shutdown( void );

sdfFontHandle_t SDF_LoadFont( const char *name, const char *atlasImage, const char *metricsFile );
void            SDF_FreeFont( sdfFontHandle_t handle );

void            SDF_DrawText( sdfFontHandle_t font, float x, float y, float scale,
                              const char *text, const float *color, const sdfDrawParams_t *params );
float           SDF_TextWidth( sdfFontHandle_t font, float scale, const char *text );
float           SDF_TextHeight( sdfFontHandle_t font, float scale );

typedef enum {
	SDF_COORDS_VIRTUAL_640 = 0,
	SDF_COORDS_SCREEN = 1
} sdfCoordSpace_t;

qboolean        SDF_IsEnabled( void );
qboolean        SDF_DrawStringExt( int x, int y, float size, const char *string,
                                   const float *setColor, qboolean forceColor, qboolean noColorEscape,
                                   sdfCoordSpace_t coordSpace );

#endif /* CL_SDF_FONT_H */
