/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Material-blend vertex weight paint sidecar (maps/<map>.paint) and brush API.
===========================================================================
*/

#ifndef TR_MATERIAL_PAINT_H
#define TR_MATERIAL_PAINT_H

#include "q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MATERIAL_PAINT_MAGIC		0x50334449u /* 'ID3P' little-endian */
#define MATERIAL_PAINT_VERSION		2
#define MATERIAL_PAINT_FLAG_STREAM2	1u

extern cvar_t *r_materialPaint;
extern cvar_t *r_materialPaintRadius;
extern cvar_t *r_materialPaintStrength;
extern cvar_t *r_materialPaintChannels;
extern cvar_t *r_materialPaintDirty;

typedef struct materialPaintVert_s {
	uint32_t	surfIndex;
	uint32_t	vertIndex;
	byte		rgba[4];
	byte		rgba2[4]; /* layers 4..7 when FLAG_STREAM2 */
} materialPaintVert_t;

void R_MaterialPaint_RegisterCvars( void );
void R_MaterialPaint_RegisterCommands( void );
void R_MaterialPaint_Init( void );
void R_MaterialPaint_Shutdown( void );

/* Apply sidecar after world surfaces exist; before or after VBO rebuild. */
void R_MaterialPaint_OnMapLoad( const char *mapBaseName );
void R_MaterialPaint_Clear( void );

qboolean R_MaterialPaint_Save( const char *pathOrNull );
qboolean R_MaterialPaint_Load( const char *pathOrNull );

/* Brush: paint world verts near worldPos. channelMask bits 0..3 = RGBA stream0; 4..7 = stream1. */
void R_MaterialPaint_Brush( const vec3_t worldPos, float radius, float strength, uint32_t channelMask, const byte targetRGBA[4] );

/* Screen-space brush using viewParms (mouse NDC -1..1). Returns qtrue if hit. */
qboolean R_MaterialPaint_BrushFromScreen( float ndcX, float ndcY, float radius, float strength, uint32_t channelMask, const byte targetRGBA[4] );

void R_MaterialPaint_InvalidateWorldVBO( void );

int R_MaterialPaint_NumVerts( void );
qboolean R_MaterialPaint_HasStream2( void );

/* Fill tess.vertexColors1 from sidecar stream2 for a world surface (surfIndex). */
void R_MaterialPaint_FillStream2ForSurface( int surfIndex, int firstVert, int numVerts );
void R_MaterialPaint_FillStream2FromSurfaceData( const void *surfData, int firstVert, int numVerts );

/* Lookup stream2 RGBA for one vert; returns qfalse if missing. */
qboolean R_MaterialPaint_GetStream2( uint32_t surfIndex, uint32_t vertIndex, byte rgba2[4] );

/* MD3 bind-pose paint overlay */
qboolean R_MaterialPaint_LoadMD3( const char *modelName, byte **outColors, int *outNumVerts );
void R_MaterialPaint_FreeMD3( byte *colors );

#ifdef __cplusplus
}
#endif

#endif
