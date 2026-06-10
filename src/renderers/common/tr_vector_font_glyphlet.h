/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Loop & Blinn glyphlet mesh data (AMD GPUOpen / mesh-shader path).
===========================================================================
*/

#ifndef TR_VECTOR_FONT_GLYPHLET_H
#define TR_VECTOR_FONT_GLYPHLET_H

#include "../../qcommon/q_shared.h"
#include "tr_vector_font.h"

#define VECTOR_GLYPHLET_ATLAS_MAX_VERTS   (65536u)
#define VECTOR_GLYPHLET_ATLAS_MAX_INDICES (196608u)
#define VECTOR_GLYPHLET_ATLAS_MAX_TRIS    (65536u)
#define VECTOR_GLYPHLET_MAX_CONTOUR_PTS   48
#define VECTOR_GLYPHLET_MAX_CONTOURS      8

typedef struct {
	float x;
	float y;
	float canonU;
	float canonV;
} vectorGlyphletVert_t;

typedef struct {
	vectorGlyphletVert_t *verts;
	uint32_t             *indices;
	uint8_t              *primTypes;
	uint32_t              vertCount;
	uint32_t              indexCount;
	uint32_t              primCount;
	uint32_t              vertCapacity;
	uint32_t              indexCapacity;
	uint32_t              primCapacity;
} vectorGlyphletAtlas_t;

void    R_VectorGlyphletAtlas_Init( vectorGlyphletAtlas_t *atlas );
void    R_VectorGlyphletAtlas_Shutdown( vectorGlyphletAtlas_t *atlas );
void    R_VectorGlyphletAtlas_Clear( vectorGlyphletAtlas_t *atlas );

#ifdef BUILD_FREETYPE
struct FT_GlyphSlotRec_;
qboolean R_VectorGlyphlet_BuildFromSlot( struct FT_GlyphSlotRec_ *slot, vectorGlyphletAtlas_t *atlas,
	vectorGlyphletInfo_t *info );
#endif

#endif /* TR_VECTOR_FONT_GLYPHLET_H */
