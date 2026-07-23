/*
===========================================================================
BSP30 BSP v30 collision-map translation.

Point contents retain the render BSP's spatial leaves. Swept boxes use the
map's precomputed clipnode hulls through the existing idTech3 collision API.
This is an independent binary-format implementation with no Half-Life SDK
source or runtime dependency.
===========================================================================
*/

#include "cm_local.h"
#include "qfiles_bsp30.h"

#define BSP30_BOX_PLANES 12
#define BSP30_BOX_SIDES 6

int CM_Bsp30Contents( int contents ) {
	switch ( contents ) {
	case BSP30_CONTENTS_SOLID:
	case BSP30_CONTENTS_CLIP:
		return CONTENTS_SOLID;
	case BSP30_CONTENTS_WATER:
	case -9:  /* directional current contents are water volumes */
	case -10:
	case -11:
	case -12:
	case -13:
	case -14:
		return CONTENTS_WATER;
	case BSP30_CONTENTS_SLIME:
		return CONTENTS_SLIME;
	case BSP30_CONTENTS_LAVA:
		return CONTENTS_LAVA;
	default:
		return 0;
	}
}

static const void *CM_Bsp30Lump( const byte *buffer, int length,
		const bsp30_header_t *header, int lumpIndex, size_t elementSize,
		int *count, const char *name ) {
	int ofs = LittleLong( header->lumps[lumpIndex].fileofs );
	int len = LittleLong( header->lumps[lumpIndex].filelen );

	if ( ofs < 0 || len < 0 || ofs > length || len > length - ofs ) {
		Com_Error( ERR_DROP, "%s: %s has invalid BSP30 lump %d", __func__, name, lumpIndex );
	}
	if ( elementSize && (size_t)len % elementSize != 0 ) {
		Com_Error( ERR_DROP, "%s: %s has malformed BSP30 lump %d", __func__, name, lumpIndex );
	}
	if ( count ) {
		*count = elementSize ? (int)((size_t)len / elementSize) : len;
	}
	return buffer + ofs;
}

static void CM_Bsp30AssignModelContents( void ) {
	const char *parse = cm.entityString;
	const char *token;

	while ( parse && *( token = COM_Parse( &parse ) ) ) {
		char classname[MAX_TOKEN_CHARS] = "";
		char model[MAX_TOKEN_CHARS] = "";
		if ( token[0] != '{' ) {
			break;
		}

		while ( parse && *( token = COM_Parse( &parse ) ) && token[0] != '}' ) {
			char key[MAX_TOKEN_CHARS];
			Q_strncpyz( key, token, sizeof( key ) );
			token = COM_Parse( &parse );
			if ( !token[0] ) {
				break;
			}
			if ( !Q_stricmp( key, "classname" ) ) {
				Q_strncpyz( classname, token, sizeof( classname ) );
			}
			else if ( !Q_stricmp( key, "model" ) ) {
				Q_strncpyz( model, token, sizeof( model ) );
			}
		}

		if ( model[0] == '*' ) {
			int modelIndex = atoi( model + 1 );
			if ( modelIndex > 0 && modelIndex < cm.numSubModels ) {
				if ( !Q_stricmp( classname, "func_water" ) ) {
					cm.cmodels[modelIndex].bsp30Contents = CONTENTS_WATER;
				}
				else if ( !Q_stricmp( classname, "func_illusionary" ) ) {
					cm.cmodels[modelIndex].bsp30Contents = 0;
				}
				else if ( !Q_stricmp( classname, "func_ladder" ) ) {
					cm.cmodels[modelIndex].bsp30Contents = CONTENTS_SOLID | CONTENTS_DONOTENTER;
				}
				else if ( !Q_stricmpn( classname, "trigger_", 8 ) ) {
					cm.cmodels[modelIndex].bsp30Contents = CONTENTS_TRIGGER;
				}
			}
		}
	}
}

void CM_LoadBSP30Map( const byte *buffer, int length, const char *name ) {
	const bsp30_header_t *header = (const bsp30_header_t *)buffer;
	const bsp30_plane_t *inPlanes;
	const bsp30_node_t *inNodes;
	const bsp30_clipnode_t *inClipNodes;
	const bsp30_leaf_t *inLeafs;
	const bsp30_model_t *inModels;
	const bsp30_face_t *inFaces;
	const byte *entities;
	int numPlanes, numNodes, numClipNodes, numLeafs, numModels, numFaces, entityLen;
	int i, j;
	int worldFacePlanes = 0;
	int hullFacePlanes = 0;
	int bevelPlanes = 0;
	int axialBevelPlanes = 0;

	if ( length < (int)sizeof( *header ) || LittleLong( header->version ) != BSP30_BSP_VERSION ) {
		Com_Error( ERR_DROP, "%s: %s is not a BSP30 BSP v30", __func__, name );
	}

	inPlanes = CM_Bsp30Lump( buffer, length, header, BSP30_LUMP_PLANES,
			sizeof( *inPlanes ), &numPlanes, name );
	inNodes = CM_Bsp30Lump( buffer, length, header, BSP30_LUMP_NODES,
			sizeof( *inNodes ), &numNodes, name );
	inClipNodes = CM_Bsp30Lump( buffer, length, header, BSP30_LUMP_CLIPNODES,
			sizeof( *inClipNodes ), &numClipNodes, name );
	inLeafs = CM_Bsp30Lump( buffer, length, header, BSP30_LUMP_LEAFS,
			sizeof( *inLeafs ), &numLeafs, name );
	inModels = CM_Bsp30Lump( buffer, length, header, BSP30_LUMP_MODELS,
			sizeof( *inModels ), &numModels, name );
	inFaces = CM_Bsp30Lump( buffer, length, header, BSP30_LUMP_FACES,
			sizeof( *inFaces ), &numFaces, name );
	entities = CM_Bsp30Lump( buffer, length, header, BSP30_LUMP_ENTITIES,
			1, &entityLen, name );

	if ( numPlanes <= 0 || numNodes <= 0 || numLeafs <= 0 || numModels <= 0 ) {
		Com_Error( ERR_DROP, "%s: %s has incomplete BSP30 collision data", __func__, name );
	}
	if ( numModels > MAX_SUBMODELS ) {
		Com_Error( ERR_DROP, "%s: %s has %d inline models (maximum %d)",
				__func__, name, numModels, MAX_SUBMODELS );
	}

	cm.bsp30 = qtrue;
	for ( i = 0; i < BSP30_MAX_MAP_HULLS; i++ ) {
		cm.bsp30WorldHeadnodes[i] = LittleLong( inModels[0].headnode[i] );
	}

	cm.numShaders = 1;
	cm.shaders = Hunk_Alloc( sizeof( *cm.shaders ), h_high );
	Q_strncpyz( cm.shaders[0].shader, "textures/bsp30/default", sizeof( cm.shaders[0].shader ) );
	cm.shaders[0].contentFlags = CONTENTS_SOLID;

	cm.numPlanes = numPlanes;
	cm.planes = Hunk_Alloc( ( numPlanes + BSP30_BOX_PLANES ) * sizeof( *cm.planes ), h_high );
	cm.bsp30PlaneKind = Hunk_Alloc( ( numPlanes + BSP30_BOX_PLANES ) *
			sizeof( *cm.bsp30PlaneKind ), h_high );
	for ( i = 0; i < numPlanes; i++ ) {
		cplane_t *out = &cm.planes[i];
		for ( j = 0; j < 3; j++ ) {
			out->normal[j] = LittleFloat( inPlanes[i].normal[j] );
		}
		out->dist = LittleFloat( inPlanes[i].dist );
		out->type = PlaneTypeForNormal( out->normal );
		SetPlaneSignbits( out );
		/*
		 * Default clipnode-only planes to bevel kinds. Faces below promote any
		 * plane that also appears on rendered geometry to WORLD_FACE.
		 */
		if ( out->type == PLANE_X || out->type == PLANE_Y ||
				out->type == PLANE_Z ) {
			cm.bsp30PlaneKind[i] = (byte)TRACE_PLANE_AXIAL_BEVEL;
		}
		else {
			cm.bsp30PlaneKind[i] = (byte)TRACE_PLANE_BEVEL;
		}
	}

	for ( i = 0; i < numFaces; i++ ) {
		int planeNum = LittleShort( inFaces[i].planenum );
		if ( planeNum < 0 || planeNum >= numPlanes ) {
			Com_Error( ERR_DROP, "%s: %s face %d has invalid plane %d",
					__func__, name, i, planeNum );
		}
		cm.bsp30PlaneKind[planeNum] = (byte)TRACE_PLANE_WORLD_FACE;
	}

	/*
	 * Clipnode-only planes that match a world face after classic hull expansion
	 * (same normal, dist ≈ face_dist + support(halfExtents)) are HULL_FACE.
	 * Normal-only matching was wrong: any +X/+Y/+Z world face re-labeled every
	 * axial clip bevel as HULL_FACE, hiding real bevels from surf diagnostics.
	 */
	{
		static const vec3_t hullHalfExtents[] = {
			{ 16.0f, 16.0f, 36.0f }, /* hull 1 stand */
			{ 32.0f, 32.0f, 32.0f }, /* hull 2 */
			{ 16.0f, 16.0f, 18.0f }  /* hull 3 duck */
		};
		const float distEps = 0.5f;
		unsigned h;

		for ( i = 0; i < numPlanes; i++ ) {
			if ( cm.bsp30PlaneKind[i] == (byte)TRACE_PLANE_WORLD_FACE ) {
				continue;
			}
			for ( j = 0; j < numPlanes; j++ ) {
				float ndot;

				if ( cm.bsp30PlaneKind[j] != (byte)TRACE_PLANE_WORLD_FACE ) {
					continue;
				}
				ndot = DotProduct( cm.planes[i].normal, cm.planes[j].normal );
				if ( ndot <= 0.9995f ) {
					continue;
				}
				for ( h = 0; h < 3; h++ ) {
					float expand =
							fabsf( cm.planes[j].normal[0] ) * hullHalfExtents[h][0] +
							fabsf( cm.planes[j].normal[1] ) * hullHalfExtents[h][1] +
							fabsf( cm.planes[j].normal[2] ) * hullHalfExtents[h][2];
					if ( fabsf( cm.planes[i].dist -
							( cm.planes[j].dist + expand ) ) <= distEps ) {
						cm.bsp30PlaneKind[i] = (byte)TRACE_PLANE_HULL_FACE;
						break;
					}
				}
				if ( cm.bsp30PlaneKind[i] == (byte)TRACE_PLANE_HULL_FACE ) {
					break;
				}
			}
		}
	}

	for ( i = 0; i < numPlanes; i++ ) {
		if ( cm.bsp30PlaneKind[i] == (byte)TRACE_PLANE_WORLD_FACE ) {
			worldFacePlanes++;
		}
		else if ( cm.bsp30PlaneKind[i] == (byte)TRACE_PLANE_HULL_FACE ) {
			hullFacePlanes++;
		}
		else if ( cm.bsp30PlaneKind[i] == (byte)TRACE_PLANE_AXIAL_BEVEL ) {
			axialBevelPlanes++;
		}
		else {
			bevelPlanes++;
		}
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

	cm.numBSP30ClipNodes = numClipNodes;
	cm.bsp30ClipNodes = Hunk_Alloc( MAX( numClipNodes, 1 ) * sizeof( *cm.bsp30ClipNodes ), h_high );
	for ( i = 0; i < numClipNodes; i++ ) {
		int planeNum = LittleLong( inClipNodes[i].planenum );
		if ( planeNum < 0 || planeNum >= numPlanes ) {
			Com_Error( ERR_DROP, "%s: %s clipnode %d has invalid plane %d", __func__, name, i, planeNum );
		}
		cm.bsp30ClipNodes[i].plane = &cm.planes[planeNum];
		for ( j = 0; j < 2; j++ ) {
			int child = LittleShort( inClipNodes[i].children[j] );
			if ( child >= numClipNodes ) {
				Com_Error( ERR_DROP, "%s: %s clipnode %d has invalid child %d", __func__, name, i, child );
			}
			cm.bsp30ClipNodes[i].children[j] = child;
		}
	}

	cm.numLeafs = numLeafs;
	cm.leafs = Hunk_Alloc( ( numLeafs + 2 ) * sizeof( *cm.leafs ), h_high );
	for ( i = 0; i < numLeafs; i++ ) {
		cm.leafs[i].contents = CM_Bsp30Contents( LittleLong( inLeafs[i].contents ) );
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
		for ( j = 0; j < BSP30_MAX_MAP_HULLS; j++ ) {
			cm.cmodels[i].bsp30Headnodes[j] = LittleLong( inModels[i].headnode[j] );
		}
		cm.cmodels[i].bsp30Contents = CONTENTS_SOLID;
	}

	cm.entityString = Hunk_Alloc( entityLen + 1, h_high );
	Com_Memcpy( cm.entityString, entities, entityLen );
	cm.entityString[entityLen] = '\0';
	cm.numEntityChars = entityLen + 1;
	CM_Bsp30AssignModelContents();

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
	cm.brushsides = Hunk_Alloc( BSP30_BOX_SIDES * sizeof( *cm.brushsides ), h_high );
	cm.numLeafBrushes = 0;
	cm.leafbrushes = Hunk_Alloc( sizeof( *cm.leafbrushes ), h_high );
	cm.numLeafSurfaces = 0;
	cm.leafsurfaces = Hunk_Alloc( sizeof( *cm.leafsurfaces ), h_high );
	cm.numSurfaces = 0;
	cm.surfaces = Hunk_Alloc( sizeof( *cm.surfaces ), h_high );

	Com_Printf( "BSP30 BSP v30 collision: %d nodes, %d clipnodes, %d leaves, "
			"%d inline models, planes=%d (world=%d hull=%d bevel=%d axial=%d)\n",
			numNodes, numClipNodes, numLeafs, numModels, numPlanes,
			worldFacePlanes, hullFacePlanes, bevelPlanes, axialBevelPlanes );
}
