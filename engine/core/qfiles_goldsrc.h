/*
===========================================================================
GoldSrc / Half-Life BSP v30 on-disk structures.

These structures are intentionally isolated from qfiles.h because the two
formats use several of the same historic type names with different layouts.
===========================================================================
*/

#ifndef QFILES_GOLDSRC_H
#define QFILES_GOLDSRC_H

#include <stdint.h>

#define GOLDSRC_BSP_VERSION 30
#define GOLDSRC_HEADER_LUMPS 15
#define GOLDSRC_MAX_MAP_HULLS 4
#define GOLDSRC_LIGHT_STYLES 4

enum {
	GOLDSRC_LUMP_ENTITIES = 0,
	GOLDSRC_LUMP_PLANES,
	GOLDSRC_LUMP_TEXTURES,
	GOLDSRC_LUMP_VERTEXES,
	GOLDSRC_LUMP_VISIBILITY,
	GOLDSRC_LUMP_NODES,
	GOLDSRC_LUMP_TEXINFO,
	GOLDSRC_LUMP_FACES,
	GOLDSRC_LUMP_LIGHTING,
	GOLDSRC_LUMP_CLIPNODES,
	GOLDSRC_LUMP_LEAFS,
	GOLDSRC_LUMP_MARKSURFACES,
	GOLDSRC_LUMP_EDGES,
	GOLDSRC_LUMP_SURFEDGES,
	GOLDSRC_LUMP_MODELS
};

enum {
	GOLDSRC_CONTENTS_EMPTY = -1,
	GOLDSRC_CONTENTS_SOLID = -2,
	GOLDSRC_CONTENTS_WATER = -3,
	GOLDSRC_CONTENTS_SLIME = -4,
	GOLDSRC_CONTENTS_LAVA = -5,
	GOLDSRC_CONTENTS_SKY = -6,
	GOLDSRC_CONTENTS_ORIGIN = -7,
	GOLDSRC_CONTENTS_CLIP = -8
};

typedef struct {
	int32_t fileofs;
	int32_t filelen;
} goldsrc_lump_t;

typedef struct {
	int32_t version;
	goldsrc_lump_t lumps[GOLDSRC_HEADER_LUMPS];
} goldsrc_header_t;

typedef struct {
	float normal[3];
	float dist;
	int32_t type;
} goldsrc_plane_t;

typedef struct {
	int32_t planenum;
	int16_t children[2];
	int16_t mins[3];
	int16_t maxs[3];
	uint16_t firstface;
	uint16_t numfaces;
} goldsrc_node_t;

typedef struct {
	int32_t contents;
	int32_t visofs;
	int16_t mins[3];
	int16_t maxs[3];
	uint16_t firstmarksurface;
	uint16_t nummarksurfaces;
	uint8_t ambient_level[4];
} goldsrc_leaf_t;

typedef struct {
	float mins[3];
	float maxs[3];
	float origin[3];
	int32_t headnode[GOLDSRC_MAX_MAP_HULLS];
	int32_t visleafs;
	int32_t firstface;
	int32_t numfaces;
} goldsrc_model_t;

typedef struct {
	float point[3];
} goldsrc_vertex_t;

typedef struct {
	float vecs[2][4];
	int32_t miptex;
	int32_t flags;
} goldsrc_texinfo_t;

typedef struct {
	uint16_t planenum;
	int16_t side;
	int32_t firstedge;
	int16_t numedges;
	int16_t texinfo;
	uint8_t styles[GOLDSRC_LIGHT_STYLES];
	int32_t lightofs;
} goldsrc_face_t;

typedef struct {
	uint16_t v[2];
} goldsrc_edge_t;

typedef struct {
	char name[16];
	uint32_t width;
	uint32_t height;
	uint32_t offsets[4];
} goldsrc_miptex_t;

#endif
