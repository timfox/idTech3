/*
===========================================================================
BSP30 / Half-Life BSP v30 and WAD3 on-disk structures.

These structures are intentionally isolated from qfiles.h because the two
formats use several of the same historic type names with different layouts.

This is a clean-room file-format description.  It has no dependency on, and
contains no source copied from, the Half-Life SDK.
===========================================================================
*/

#ifndef QFILES_BSP30_H
#define QFILES_BSP30_H

#include <stdint.h>

#define BSP30_BSP_VERSION 30
#define BSP30_HEADER_LUMPS 15
#define BSP30_MAX_MAP_HULLS 4
#define BSP30_LIGHT_STYLES 4

enum {
	BSP30_LUMP_ENTITIES = 0,
	BSP30_LUMP_PLANES,
	BSP30_LUMP_TEXTURES,
	BSP30_LUMP_VERTEXES,
	BSP30_LUMP_VISIBILITY,
	BSP30_LUMP_NODES,
	BSP30_LUMP_TEXINFO,
	BSP30_LUMP_FACES,
	BSP30_LUMP_LIGHTING,
	BSP30_LUMP_CLIPNODES,
	BSP30_LUMP_LEAFS,
	BSP30_LUMP_MARKSURFACES,
	BSP30_LUMP_EDGES,
	BSP30_LUMP_SURFEDGES,
	BSP30_LUMP_MODELS
};

enum {
	BSP30_CONTENTS_EMPTY = -1,
	BSP30_CONTENTS_SOLID = -2,
	BSP30_CONTENTS_WATER = -3,
	BSP30_CONTENTS_SLIME = -4,
	BSP30_CONTENTS_LAVA = -5,
	BSP30_CONTENTS_SKY = -6,
	BSP30_CONTENTS_ORIGIN = -7,
	BSP30_CONTENTS_CLIP = -8
};

typedef struct {
	int32_t fileofs;
	int32_t filelen;
} bsp30_lump_t;

typedef struct {
	int32_t version;
	bsp30_lump_t lumps[BSP30_HEADER_LUMPS];
} bsp30_header_t;

typedef struct {
	float normal[3];
	float dist;
	int32_t type;
} bsp30_plane_t;

typedef struct {
	int32_t planenum;
	int16_t children[2];
	int16_t mins[3];
	int16_t maxs[3];
	uint16_t firstface;
	uint16_t numfaces;
} bsp30_node_t;

typedef struct {
	int32_t planenum;
	int16_t children[2];
} bsp30_clipnode_t;

typedef struct {
	int32_t contents;
	int32_t visofs;
	int16_t mins[3];
	int16_t maxs[3];
	uint16_t firstmarksurface;
	uint16_t nummarksurfaces;
	uint8_t ambient_level[4];
} bsp30_leaf_t;

typedef struct {
	float mins[3];
	float maxs[3];
	float origin[3];
	int32_t headnode[BSP30_MAX_MAP_HULLS];
	int32_t visleafs;
	int32_t firstface;
	int32_t numfaces;
} bsp30_model_t;

typedef struct {
	float point[3];
} bsp30_vertex_t;

typedef struct {
	float vecs[2][4];
	int32_t miptex;
	int32_t flags;
} bsp30_texinfo_t;

typedef struct {
	uint16_t planenum;
	int16_t side;
	int32_t firstedge;
	int16_t numedges;
	int16_t texinfo;
	uint8_t styles[BSP30_LIGHT_STYLES];
	int32_t lightofs;
} bsp30_face_t;

typedef struct {
	uint16_t v[2];
} bsp30_edge_t;

typedef struct {
	char name[16];
	uint32_t width;
	uint32_t height;
	uint32_t offsets[4];
} bsp30_miptex_t;

#define BSP30_WAD3_ID "WAD3"

typedef struct {
	char identification[4];
	int32_t numlumps;
	int32_t infotableofs;
} bsp30_wad_header_t;

typedef struct {
	int32_t filepos;
	int32_t disksize;
	int32_t size;
	uint8_t type;
	uint8_t compression;
	uint8_t padding[2];
	char name[16];
} bsp30_wad_lump_t;

#endif
