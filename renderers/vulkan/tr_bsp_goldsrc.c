/*
===========================================================================
GoldSrc / Half-Life BSP v30 renderer bridge.

GoldSrc faces are reconstructed from edges and surfedges and submitted as
ordinary idTech3 planar surfaces. Embedded indexed textures are expanded to
RGBA at load time. GoldSrc lightmaps use a different packing model, so this
initial bridge uses vertex-white lighting while retaining the original base
textures and geometry.
===========================================================================
*/

#include "tr_local.h"
#include "../../engine/core/qfiles_goldsrc.h"

typedef struct {
	const byte *base;
	int size;
	const goldsrc_header_t *header;
	world_t *world;
	shader_t **textureShaders;
	int *textureWidths;
	int *textureHeights;
	int numTextures;
} goldsrcRenderLoad_t;

static int GS_LumpLength( const goldsrcRenderLoad_t *load, int lump ) {
	return LittleLong( load->header->lumps[lump].filelen );
}

static const byte *GS_LumpData( const goldsrcRenderLoad_t *load, int lump ) {
	return load->base + LittleLong( load->header->lumps[lump].fileofs );
}

static int GS_LumpCount( const goldsrcRenderLoad_t *load, int lump, int elementSize ) {
	int length = GS_LumpLength( load, lump );
	if ( elementSize <= 0 || length % elementSize ) {
		ri.Error( ERR_DROP, "%s: malformed GoldSrc lump %d", __func__, lump );
	}
	return length / elementSize;
}

static void GS_ValidateHeader( const goldsrcRenderLoad_t *load, const char *mapname ) {
	int i;

	if ( load->size < (int)sizeof( goldsrc_header_t ) ||
			LittleLong( load->header->version ) != GOLDSRC_BSP_VERSION ) {
		ri.Error( ERR_DROP, "%s: %s is not a GoldSrc BSP v30 map", __func__, mapname );
	}

	for ( i = 0; i < GOLDSRC_HEADER_LUMPS; i++ ) {
		int offset = LittleLong( load->header->lumps[i].fileofs );
		int length = LittleLong( load->header->lumps[i].filelen );
		if ( offset < 0 || length < 0 || offset > load->size || length > load->size - offset ) {
			ri.Error( ERR_DROP, "%s: %s has invalid GoldSrc lump %d", __func__, mapname, i );
		}
	}
}

static void GS_TextureName( char *out, int outSize, const goldsrc_miptex_t *miptex, int index ) {
	char raw[17];
	Com_Memcpy( raw, miptex->name, 16 );
	raw[16] = '\0';
	if ( raw[0] ) {
		Com_sprintf( out, outSize, "*goldsrc/%s", raw );
	}
	else {
		Com_sprintf( out, outSize, "*goldsrc/texture_%d", index );
	}
}

static void GS_LoadTextures( goldsrcRenderLoad_t *load ) {
	const byte *lump = GS_LumpData( load, GOLDSRC_LUMP_TEXTURES );
	int lumpLength = GS_LumpLength( load, GOLDSRC_LUMP_TEXTURES );
	int numTextures;
	int i;

	if ( lumpLength < 4 ) {
		load->numTextures = 0;
		return;
	}

	numTextures = LittleLong( *(const int32_t *)lump );
	if ( numTextures < 0 || numTextures > ( lumpLength - 4 ) / 4 ) {
		ri.Error( ERR_DROP, "%s: invalid GoldSrc texture directory", __func__ );
	}

	load->numTextures = numTextures;
	load->textureShaders = ri.Hunk_Alloc( MAX( numTextures, 1 ) * sizeof( *load->textureShaders ), h_low );
	load->textureWidths = ri.Hunk_Alloc( MAX( numTextures, 1 ) * sizeof( *load->textureWidths ), h_low );
	load->textureHeights = ri.Hunk_Alloc( MAX( numTextures, 1 ) * sizeof( *load->textureHeights ), h_low );
	load->world->numShaders = numTextures;
	load->world->shaders = ri.Hunk_Alloc( MAX( numTextures, 1 ) * sizeof( *load->world->shaders ), h_low );

	for ( i = 0; i < numTextures; i++ ) {
		int textureOffset = LittleLong( ((const int32_t *)( lump + 4 ))[i] );
		const goldsrc_miptex_t *miptex;
		char shaderName[MAX_QPATH];
		uint32_t width, height, pixelOffset;
		uint64_t pixelCount, paletteOffset;
		byte *rgba;
		const byte *pixels, *palette;
		image_t *image;
		qhandle_t shaderHandle;
		int p;

		load->textureShaders[i] = tr.defaultShader;
		load->textureWidths[i] = 64;
		load->textureHeights[i] = 64;
		if ( textureOffset < 0 || textureOffset > lumpLength - (int)sizeof( *miptex ) ) {
			continue;
		}

		miptex = (const goldsrc_miptex_t *)( lump + textureOffset );
		GS_TextureName( shaderName, sizeof( shaderName ), miptex, i );
		Q_strncpyz( load->world->shaders[i].shader, shaderName,
				sizeof( load->world->shaders[i].shader ) );
		width = LittleLong( miptex->width );
		height = LittleLong( miptex->height );
		pixelOffset = LittleLong( miptex->offsets[0] );
		if ( width == 0 || height == 0 || width > 4096 || height > 4096 ) {
			continue;
		}
		load->textureWidths[i] = (int)width;
		load->textureHeights[i] = (int)height;
		pixelCount = (uint64_t)width * height;
		paletteOffset = (uint64_t)LittleLong( miptex->offsets[3] ) + pixelCount / 64 + 2;
		if ( pixelOffset == 0 || pixelCount > INT_MAX ||
				(uint64_t)textureOffset + pixelOffset + pixelCount > (uint64_t)lumpLength ||
				(uint64_t)textureOffset + paletteOffset + 256 * 3 > (uint64_t)lumpLength ) {
			continue;
		}

		pixels = (const byte *)miptex + pixelOffset;
		palette = (const byte *)miptex + paletteOffset;
		rgba = ri.Hunk_AllocateTempMemory( (int)pixelCount * 4 );
		for ( p = 0; p < (int)pixelCount; p++ ) {
			int colorIndex = pixels[p];
			rgba[p * 4 + 0] = palette[colorIndex * 3 + 0];
			rgba[p * 4 + 1] = palette[colorIndex * 3 + 1];
			rgba[p * 4 + 2] = palette[colorIndex * 3 + 2];
			rgba[p * 4 + 3] = ( miptex->name[0] == '{' && colorIndex == 255 ) ? 0 : 255;
		}

		image = R_CreateImage( shaderName, NULL, rgba, (int)width, (int)height,
				IMGFLAG_MIPMAP | IMGFLAG_NO_COMPRESSION, 0, 0 );
		ri.Hunk_FreeTempMemory( rgba );
		shaderHandle = RE_RegisterShaderFromImage( shaderName, LIGHTMAP_BY_VERTEX, image, qfalse );
		load->textureShaders[i] = R_GetShaderByHandle( shaderHandle );
	}
}

static void GS_LoadPlanes( goldsrcRenderLoad_t *load ) {
	const goldsrc_plane_t *input = (const goldsrc_plane_t *)GS_LumpData( load, GOLDSRC_LUMP_PLANES );
	int count = GS_LumpCount( load, GOLDSRC_LUMP_PLANES, sizeof( *input ) );
	int i, j;

	load->world->numplanes = count;
	load->world->planes = ri.Hunk_Alloc( MAX( count, 1 ) * sizeof( *load->world->planes ), h_low );
	for ( i = 0; i < count; i++ ) {
		cplane_t *plane = &load->world->planes[i];
		for ( j = 0; j < 3; j++ ) {
			plane->normal[j] = LittleFloat( input[i].normal[j] );
		}
		plane->dist = LittleFloat( input[i].dist );
		plane->type = PlaneTypeForNormal( plane->normal );
		SetPlaneSignbits( plane );
	}
}

static int GS_SurfaceVertex( const goldsrcRenderLoad_t *load, int surfedgeIndex ) {
	const int32_t *surfedges = (const int32_t *)GS_LumpData( load, GOLDSRC_LUMP_SURFEDGES );
	const goldsrc_edge_t *edges = (const goldsrc_edge_t *)GS_LumpData( load, GOLDSRC_LUMP_EDGES );
	int numSurfedges = GS_LumpCount( load, GOLDSRC_LUMP_SURFEDGES, sizeof( *surfedges ) );
	int numEdges = GS_LumpCount( load, GOLDSRC_LUMP_EDGES, sizeof( *edges ) );
	int edgeIndex;

	if ( surfedgeIndex < 0 || surfedgeIndex >= numSurfedges ) {
		ri.Error( ERR_DROP, "%s: invalid GoldSrc surfedge", __func__ );
	}
	edgeIndex = LittleLong( surfedges[surfedgeIndex] );
	if ( edgeIndex >= 0 ) {
		if ( edgeIndex >= numEdges ) {
			ri.Error( ERR_DROP, "%s: invalid GoldSrc edge", __func__ );
		}
		return LittleShort( edges[edgeIndex].v[0] );
	}
	edgeIndex = -edgeIndex;
	if ( edgeIndex >= numEdges ) {
		ri.Error( ERR_DROP, "%s: invalid GoldSrc edge", __func__ );
	}
	return LittleShort( edges[edgeIndex].v[1] );
}

static void GS_LoadSurfaces( goldsrcRenderLoad_t *load ) {
	const goldsrc_face_t *faces = (const goldsrc_face_t *)GS_LumpData( load, GOLDSRC_LUMP_FACES );
	const goldsrc_vertex_t *vertices = (const goldsrc_vertex_t *)GS_LumpData( load, GOLDSRC_LUMP_VERTEXES );
	const goldsrc_texinfo_t *texinfos = (const goldsrc_texinfo_t *)GS_LumpData( load, GOLDSRC_LUMP_TEXINFO );
	int numFaces = GS_LumpCount( load, GOLDSRC_LUMP_FACES, sizeof( *faces ) );
	int numVertices = GS_LumpCount( load, GOLDSRC_LUMP_VERTEXES, sizeof( *vertices ) );
	int numTexinfos = GS_LumpCount( load, GOLDSRC_LUMP_TEXINFO, sizeof( *texinfos ) );
	int numPlanes = load->world->numplanes;
	int i;

	load->world->numsurfaces = numFaces;
	load->world->surfaces = ri.Hunk_Alloc( MAX( numFaces, 1 ) * sizeof( *load->world->surfaces ), h_low );
	for ( i = 0; i < numFaces; i++ ) {
		msurface_t *surface = &load->world->surfaces[i];
		int numPoints = LittleShort( faces[i].numedges );
		int numIndices = MAX( numPoints - 2, 0 ) * 3;
		int firstEdge = LittleLong( faces[i].firstedge );
		int texinfoIndex = LittleShort( faces[i].texinfo );
		int planeIndex = LittleShort( faces[i].planenum );
		int side = LittleShort( faces[i].side );
		int allocationSize, indicesOffset;
		srfSurfaceFace_t *face;
		const goldsrc_texinfo_t *texinfo;
		int textureIndex, textureWidth = 64, textureHeight = 64;
		int j;

		if ( numPoints < 3 || numPoints > 4096 || texinfoIndex < 0 || texinfoIndex >= numTexinfos ||
				planeIndex < 0 || planeIndex >= numPlanes ) {
			ri.Error( ERR_DROP, "%s: invalid GoldSrc face %d", __func__, i );
		}
		indicesOffset = sizeof( *face ) - sizeof( face->points ) + sizeof( face->points[0] ) * numPoints;
		allocationSize = indicesOffset + sizeof( int ) * numIndices;
		face = ri.Hunk_Alloc( allocationSize, h_low );
		face->surfaceType = SF_FACE;
		face->numPoints = numPoints;
		face->numIndices = numIndices;
		face->ofsIndices = indicesOffset;
		face->plane = load->world->planes[planeIndex];
		if ( side ) {
			VectorNegate( face->plane.normal, face->plane.normal );
			face->plane.dist = -face->plane.dist;
			face->plane.type = PlaneTypeForNormal( face->plane.normal );
			SetPlaneSignbits( &face->plane );
		}

		texinfo = &texinfos[texinfoIndex];
		textureIndex = LittleLong( texinfo->miptex );
		if ( textureIndex >= 0 && textureIndex < load->numTextures ) {
			surface->shader = load->textureShaders[textureIndex];
			textureWidth = load->textureWidths[textureIndex];
			textureHeight = load->textureHeights[textureIndex];
		}
		else {
			surface->shader = tr.defaultShader;
		}
		surface->fogIndex = 0;

		for ( j = 0; j < numPoints; j++ ) {
			int vertexIndex = GS_SurfaceVertex( load, firstEdge + j );
			float *point = face->points[j];
			byte white[4] = { 255, 255, 255, 255 };
			int k;
			if ( vertexIndex < 0 || vertexIndex >= numVertices ) {
				ri.Error( ERR_DROP, "%s: invalid GoldSrc vertex", __func__ );
			}
			for ( k = 0; k < 3; k++ ) {
				point[k] = LittleFloat( vertices[vertexIndex].point[k] );
#ifdef USE_VK_PBR
				point[3 + k] = face->plane.normal[k];
#endif
			}
#ifdef USE_VK_PBR
			point[6] = ( DotProduct( point, texinfo->vecs[0] ) + LittleFloat( texinfo->vecs[0][3] ) ) / textureWidth;
			point[7] = ( DotProduct( point, texinfo->vecs[1] ) + LittleFloat( texinfo->vecs[1][3] ) ) / textureHeight;
			point[8] = point[9] = 0.0f;
			R_ColorShiftLightingBytes( white, (byte *)&point[10], qtrue );
#else
			point[3] = ( DotProduct( point, texinfo->vecs[0] ) + LittleFloat( texinfo->vecs[0][3] ) ) / textureWidth;
			point[4] = ( DotProduct( point, texinfo->vecs[1] ) + LittleFloat( texinfo->vecs[1][3] ) ) / textureHeight;
			point[5] = point[6] = 0.0f;
			R_ColorShiftLightingBytes( white, (byte *)&point[7], qtrue );
#endif
		}

		for ( j = 0; j < numPoints - 2; j++ ) {
			int *indices = (int *)( (byte *)face + face->ofsIndices );
			indices[j * 3 + 0] = 0;
			indices[j * 3 + 1] = j + 1;
			indices[j * 3 + 2] = j + 2;
		}
#ifdef USE_VK_PBR
		vk_mikkt_bsp_face_generate( face );
#endif
		surface->data = (surfaceType_t *)face;
	}
}

static void GS_LoadMarksurfaces( goldsrcRenderLoad_t *load ) {
	const uint16_t *input = (const uint16_t *)GS_LumpData( load, GOLDSRC_LUMP_MARKSURFACES );
	int count = GS_LumpCount( load, GOLDSRC_LUMP_MARKSURFACES, sizeof( *input ) );
	int i;
	load->world->nummarksurfaces = count;
	load->world->marksurfaces = ri.Hunk_Alloc( MAX( count, 1 ) * sizeof( *load->world->marksurfaces ), h_low );
	for ( i = 0; i < count; i++ ) {
		int surfaceIndex = LittleShort( input[i] );
		if ( surfaceIndex < 0 || surfaceIndex >= load->world->numsurfaces ) {
			ri.Error( ERR_DROP, "%s: invalid GoldSrc marksurface", __func__ );
		}
		load->world->marksurfaces[i] = &load->world->surfaces[surfaceIndex];
	}
}

static void GS_SetParent( mnode_t *node, mnode_t *parent ) {
	node->parent = parent;
	if ( node->contents != (int)CONTENTS_NODE ) {
		return;
	}
	GS_SetParent( node->children[0], node );
	GS_SetParent( node->children[1], node );
}

static int GS_RenderContents( int goldsrcContents ) {
	switch ( goldsrcContents ) {
	case GOLDSRC_CONTENTS_SOLID: return CONTENTS_SOLID;
	case GOLDSRC_CONTENTS_WATER: return CONTENTS_WATER;
	case GOLDSRC_CONTENTS_SLIME: return CONTENTS_SLIME;
	case GOLDSRC_CONTENTS_LAVA: return CONTENTS_LAVA;
	default: return 0;
	}
}

static void GS_LoadNodesAndLeafs( goldsrcRenderLoad_t *load ) {
	const goldsrc_node_t *nodes = (const goldsrc_node_t *)GS_LumpData( load, GOLDSRC_LUMP_NODES );
	const goldsrc_leaf_t *leafs = (const goldsrc_leaf_t *)GS_LumpData( load, GOLDSRC_LUMP_LEAFS );
	int numNodes = GS_LumpCount( load, GOLDSRC_LUMP_NODES, sizeof( *nodes ) );
	int numLeafs = GS_LumpCount( load, GOLDSRC_LUMP_LEAFS, sizeof( *leafs ) );
	int i, j;

	load->world->numDecisionNodes = numNodes;
	load->world->numnodes = numNodes + numLeafs;
	load->world->nodes = ri.Hunk_Alloc( MAX( numNodes + numLeafs, 1 ) * sizeof( *load->world->nodes ), h_low );
	for ( i = 0; i < numNodes; i++ ) {
		mnode_t *out = &load->world->nodes[i];
		int planeIndex = LittleLong( nodes[i].planenum );
		if ( planeIndex < 0 || planeIndex >= load->world->numplanes ) {
			ri.Error( ERR_DROP, "%s: invalid GoldSrc node plane", __func__ );
		}
		out->contents = CONTENTS_NODE;
		out->plane = &load->world->planes[planeIndex];
		for ( j = 0; j < 3; j++ ) {
			out->mins[j] = LittleShort( nodes[i].mins[j] );
			out->maxs[j] = LittleShort( nodes[i].maxs[j] );
		}
		for ( j = 0; j < 2; j++ ) {
			int child = (int16_t)LittleShort( nodes[i].children[j] );
			if ( child >= 0 ) {
				if ( child >= numNodes ) ri.Error( ERR_DROP, "%s: invalid GoldSrc node child", __func__ );
				out->children[j] = &load->world->nodes[child];
			}
			else {
				int leafIndex = -1 - child;
				if ( leafIndex < 0 || leafIndex >= numLeafs ) ri.Error( ERR_DROP, "%s: invalid GoldSrc leaf child", __func__ );
				out->children[j] = &load->world->nodes[numNodes + leafIndex];
			}
		}
	}

	for ( i = 0; i < numLeafs; i++ ) {
		mnode_t *out = &load->world->nodes[numNodes + i];
		int firstMark = LittleShort( leafs[i].firstmarksurface );
		int numMarks = LittleShort( leafs[i].nummarksurfaces );
		for ( j = 0; j < 3; j++ ) {
			out->mins[j] = LittleShort( leafs[i].mins[j] );
			out->maxs[j] = LittleShort( leafs[i].maxs[j] );
		}
		out->contents = GS_RenderContents( LittleLong( leafs[i].contents ) );
		out->cluster = out->contents == CONTENTS_SOLID ? -1 : 0;
		out->area = 0;
		if ( firstMark < 0 || numMarks < 0 || firstMark > load->world->nummarksurfaces - numMarks ) {
			ri.Error( ERR_DROP, "%s: invalid GoldSrc leaf marksurfaces", __func__ );
		}
		out->firstmarksurface = load->world->marksurfaces + firstMark;
		out->nummarksurfaces = numMarks;
	}
	if ( numNodes > 0 ) {
		GS_SetParent( load->world->nodes, NULL );
	}
}

static void GS_LoadSubmodels( goldsrcRenderLoad_t *load ) {
	const goldsrc_model_t *input = (const goldsrc_model_t *)GS_LumpData( load, GOLDSRC_LUMP_MODELS );
	int count = GS_LumpCount( load, GOLDSRC_LUMP_MODELS, sizeof( *input ) );
	int i, j;

	load->world->numBModels = count;
	load->world->bmodels = ri.Hunk_Alloc( MAX( count, 1 ) * sizeof( *load->world->bmodels ), h_low );
	for ( i = 0; i < count; i++ ) {
		bmodel_t *out = &load->world->bmodels[i];
		model_t *model = R_AllocModel();
		int firstFace = LittleLong( input[i].firstface );
		int numFaces = LittleLong( input[i].numfaces );
		if ( !model ) {
			ri.Error( ERR_DROP, "%s: R_AllocModel failed", __func__ );
		}
		if ( firstFace < 0 || numFaces < 0 || firstFace > load->world->numsurfaces - numFaces ) {
			ri.Error( ERR_DROP, "%s: invalid GoldSrc submodel surfaces", __func__ );
		}
		model->type = MOD_BRUSH;
		model->bmodel = out;
		Com_sprintf( model->name, sizeof( model->name ), "*%d", i );
		for ( j = 0; j < 3; j++ ) {
			out->bounds[0][j] = LittleFloat( input[i].mins[j] );
			out->bounds[1][j] = LittleFloat( input[i].maxs[j] );
		}
		out->firstSurface = load->world->surfaces + firstFace;
		out->numSurfaces = numFaces;
	}
}

static void GS_LoadVisibilityAndEntities( goldsrcRenderLoad_t *load ) {
	const byte *entities = GS_LumpData( load, GOLDSRC_LUMP_ENTITIES );
	int entityLength = GS_LumpLength( load, GOLDSRC_LUMP_ENTITIES );
	load->world->numClusters = 1;
	load->world->clusterBytes = 1;
	load->world->vis = NULL;
	load->world->novis = ri.Hunk_Alloc( 64, h_low );
	Com_Memset( load->world->novis, 0xff, 64 );
	load->world->entityString = ri.Hunk_Alloc( entityLength + 1, h_low );
	Com_Memcpy( load->world->entityString, entities, entityLength );
	load->world->entityString[entityLength] = '\0';
	load->world->entityParsePoint = load->world->entityString;
}

void R_LoadGoldSrcWorld( const char *mapname, const byte *buffer, int size, world_t *world ) {
	goldsrcRenderLoad_t load;
	Com_Memset( &load, 0, sizeof( load ) );
	load.base = buffer;
	load.size = size;
	load.header = (const goldsrc_header_t *)buffer;
	load.world = world;

	GS_ValidateHeader( &load, mapname );
	tr.numLightmaps = 0;
	tr.mergeLightmaps = qfalse;
	tr.worldDeluxeMapping = qfalse;
	GS_LoadTextures( &load );
	GS_LoadPlanes( &load );
	GS_LoadSurfaces( &load );
	GS_LoadMarksurfaces( &load );
	GS_LoadNodesAndLeafs( &load );
	GS_LoadSubmodels( &load );
	GS_LoadVisibilityAndEntities( &load );

	ri.Printf( PRINT_ALL, "...loaded GoldSrc BSP30: %d faces, %d textures, %d models\n",
			world->numsurfaces, load.numTextures, world->numBModels );
}
