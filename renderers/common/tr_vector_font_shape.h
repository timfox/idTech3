#ifndef TR_VECTOR_FONT_SHAPE_H
#define TR_VECTOR_FONT_SHAPE_H

#include "q_shared.h"

typedef struct vectorShapedGlyph_s {
	uint32_t glyphIndex;
	uint32_t cluster;
	float xAdvance;
	float yAdvance;
	float xOffset;
	float yOffset;
	uint16_t fontSlot;
	uint16_t flags;
} vectorShapedGlyph_t;

typedef struct vectorShapedRun_s {
	vectorShapedGlyph_t *glyphs;
	uint32_t glyphCount;
	uint32_t script;
	uint32_t direction;
	uint32_t languageHash;
	uint32_t featureHash;
	uint32_t variationHash;
	float advanceX;
	float advanceY;
} vectorShapedRun_t;

qboolean R_VectorFont_ShapeRun( const char *utf8, uint32_t fontSlot,
	vectorShapedRun_t *out, vectorShapedGlyph_t *glyphBuf, uint32_t glyphCap );

#endif
