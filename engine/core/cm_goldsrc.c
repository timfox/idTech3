/*
===========================================================================
GoldSrc BSP v30 collision-map translation.

GoldSrc's render BSP already describes convex spatial leaves.  We retain that
tree and let cm_trace traverse leaf contents directly, while keeping the
existing idTech3 public collision API and temporary box-model machinery.
===========================================================================
*/

#include "cm_local.h"
#include "qfiles_goldsrc.h"

#define GOLDSRC_BOX_PLANES 12
#define GOLDSRC_BOX_SIDES 6

static int CM_GoldSrcContents( int contents ) {
	switch ( contents ) {
	case GOLDSRC_CONTENTS_SOLID:
	case GOLDSRC_CONTENTS_CLIP:
		return CONTENTS_SOLID;
	case GOLDSRC_CONTENTS_WATER:
		return CONTENTS_WATER;
	case GOLDSRC_CONTENTS_SLIME:
		return CONTENTS_SLIME;
	case GOLDSRC_CONTENTS_LAVA:
		return CONTENTS_LAVA;
	default:
		return 0;
	}
}

static const void *CM_GoldSrcLump( const byte *buffer, int length,
		const goldsrc_header_t *header, int lumpIndex, size_t elementSize,
		int *count, const char *name ) {
	int ofs = LittleLong( header->lumps[lumpIndex].fileofs );
	int len = LittleLong( header->lumps[lumpIndex].filelen );

	if ( ofs < 0 || len < 0 || ofs > length || len > length - ofs ) {
		Com_Error( ERR_DROP, "%s: %s has invalid GoldSrc lump %d", __func__, name, lumpIndex );
	}
	if ( elementSize && (size_t)len % elementSize != 0 ) {
		Com_Error( ERR_DROP, "%s: %s has malformed GoldSrc lump %d", __func__, name, lumpIndex );
	}
	if ( count ) {
		*count = elementSize ? (int)((size_t)len / elementSize) : len;
	}
	return buffer + ofs;
}

void CM_LoadGoldSrcMap( const byte *buffer, int length, const char *name ) {
	const goldsrc_header_t *header = (const goldsrc_header_t *)buffer;
	const goldsrc_plane_t *inPlanes;
	const goldsrc_node_t *inNodes;
	const goldsrc_leaf_t *inLeafs;
	const goldsrc_model_t *inModels;
	const byte *entities;
	int numPlanes, numNodes, numLeafs, numModels, entityLen;
	int i, j;

	if ( length < (int)sizeof( *header ) || LittleLong( header->version ) != GOLDSRC_BSP_VERSION ) {
		Com_Error( ERR_DROP, "%s: %s is not a GoldSrc BSP v30", __func__, name );
	}

	inPlanes = CM_GoldSrcLump( buffer, length, header, GOLDSRC_LUMP_PLANES,
			sizeof( *inPlanes ), &numPlanes, name );
	inNodes = CM_GoldSrcLump( buffer, length, header, GOLDSRC_LUMP_NODES,
			sizeof( *inNodes ), &numNodes, name );
	inLeafs = CM_GoldSrcLump( buffer, length, header, GOLDSRC_LUMP_LEAFS,
			sizeof( *inLeafs ), &numLeafs, name );
	inModels = CM_GoldSrcLump( buffer, length, header, GOLDSRC_LUMP_MODELS,
			sizeof( *inModels ), &numModels, name );
	entities = CM_GoldSrcLump( buffer, length, header, GOLDSRC_LUMP_ENTITIES,
			1, &entityLen, name );

	if ( numPlanes <= 0 || numNodes <= 0 || numLeafs <= 0 || numModels <= 0 ) {
		Com_Error( ERR_DROP, "%s: %s has incomplete GoldSrc collision data", __func__, name );
	}
	if ( numModels > MAX_SUBMODELS ) {
		Com_Error( ERR_DROP, "%s: %s has %d inline models (maximum %d)",
				__func__, name, numModels, MAX_SUBMODELS );
	}

	cm.goldsrc = qtrue;
	cm.goldsrcWorldHeadnode = LittleLong( inModels[0].headnode[0] );

	cm.numShaders = 1;
	cm.shaders = Hunk_Alloc( sizeof( *cm.shaders ), h_high );
	Q_strncpyz( cm.shaders[0].shader, "textures/goldsrc/default", sizeof( cm.shaders[0].shader ) );
	cm.shaders[0].contentFlags = CONTENTS_SOLID;

	cm.numPlanes = numPlanes;
	cm.planes = Hunk_Alloc( ( numPlanes + GOLDSRC_BOX_PLANES ) * sizeof( *cm.planes ), h_high );
	for ( i = 0; i < numPlanes; i++ ) {
		cplane_t *out = &cm.planes[i];
		for ( j = 0; j < 3; j++ ) {
			out->normal[j] = LittleFloat( inPlanes[i].normal[j] );
		}
		out->dist = LittleFloat( inPlanes[i].dist );
		out->type = PlaneTypeForNormal( out->normal );
		SetPlaneSignbits( out );
	}

	cm.numNodes = numNodes;
	cm.nodes = Hunk_Alloc( numNodes * sizeof( *cm.nodes ), h_high );
	for ( i = 0; i < numNodes; i++ ) {
		int planeNum = LittleLong( inNodes[i].planenum );
		if ( planeNum < 0 || planeNum >= numPlanes ) {
			Com_Error( ERR_DROP, "%s: %s node %d has invalid plane %d", __func__, name, i, planeNum );
		}
		cm.nodes[i].plane = &cm.planes[planeNum];
		for ( j = 0; j < 2; j++ ) {
			int child = LittleShort( inNodes[i].children[j] );
			if ( child >= numNodes || ( child < 0 && -1 - child >= numLeafs ) ) {
				Com_Error( ERR_DROP, "%s: %s node %d has invalid child %d", __func__, name, i, child );
			}
			cm.nodes[i].children[j] = child;
		}
	}

	cm.numLeafs = numLeafs;
	cm.leafs = Hunk_Alloc( ( numLeafs + 2 ) * sizeof( *cm.leafs ), h_high );
	for ( i = 0; i < numLeafs; i++ ) {
		cm.leafs[i].contents = CM_GoldSrcContents( LittleLong( inLeafs[i].contents ) );
		cm.leafs[i].cluster = cm.leafs[i].contents & CONTENTS_SOLID ? -1 : 0;
		cm.leafs[i].area = 0;
	}

	cm.numSubModels = numModels;
	cm.cmodels = Hunk_Alloc( numModels * sizeof( *cm.cmodels ), h_high );
	for ( i = 0; i < numModels; i++ ) {
		for ( j = 0; j < 3; j++ ) {
			cm.cmodels[i].mins[j] = LittleFloat( inModels[i].mins[j] ) - 1.0f;
			cm.cmodels[i].maxs[j] = LittleFloat( inModels[i].maxs[j] ) + 1.0f;
		}
		cm.cmodels[i].goldsrcHeadnode = LittleLong( inModels[i].headnode[0] );
	}

	cm.entityString = Hunk_Alloc( entityLen + 1, h_high );
	Com_Memcpy( cm.entityString, entities, entityLen );
	cm.entityString[entityLen] = '\0';
	cm.numEntityChars = entityLen + 1;

	cm.numClusters = 1;
	cm.clusterBytes = 32;
	cm.visibility = Hunk_Alloc( cm.clusterBytes, h_high );
	Com_Memset( cm.visibility, 0xff, cm.clusterBytes );
	cm.vised = qfalse;

	cm.numAreas = 1;
	cm.areas = Hunk_Alloc( sizeof( *cm.areas ), h_high );
	cm.areaPortals = Hunk_Alloc( sizeof( *cm.areaPortals ), h_high );

	/* CM_InitBoxHull appends one temporary brush to these arrays. */
	cm.numBrushes = 0;
	cm.brushes = Hunk_Alloc( sizeof( *cm.brushes ), h_high );
	cm.numBrushSides = 0;
	cm.brushsides = Hunk_Alloc( GOLDSRC_BOX_SIDES * sizeof( *cm.brushsides ), h_high );
	cm.numLeafBrushes = 0;
	cm.leafbrushes = Hunk_Alloc( sizeof( *cm.leafbrushes ), h_high );
	cm.numLeafSurfaces = 0;
	cm.leafsurfaces = Hunk_Alloc( sizeof( *cm.leafsurfaces ), h_high );
	cm.numSurfaces = 0;
	cm.surfaces = Hunk_Alloc( sizeof( *cm.surfaces ), h_high );

	Com_Printf( "GoldSrc BSP v30 collision: %d nodes, %d leaves, %d inline models\n",
			numNodes, numLeafs, numModels );
}
