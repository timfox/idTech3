/*
 * Source 1 VBSP reader.
 *
 * This is an independent implementation of the public on-disk container
 * layout.  It deliberately does not include Valve SDK headers, code, or
 * game assets.  The first pass imports static world faces and material names
 * into the existing idTech3 surface path; missing VMT/VTF content uses the
 * normal renderer fallback.
 */

#include "tr_local.h"

#define SV_MAGIC 0x50534256u /* "VBSP" little endian */
#define SV_LUMP_COUNT 64
#define SV_HEADER_BYTES 8
#define SV_LUMP_BYTES 16
#define SV_PLANE_BYTES 20
#define SV_VERTEX_BYTES 12
#define SV_EDGE_BYTES 4
#define SV_SURFEDGE_BYTES 4
#define SV_NODE_BYTES 32
#define SV_LEAF_BYTES 32
#define SV_LEAFFACE_BYTES 2
#define SV_VIS_HEADER_BYTES 4
#define SV_TEXINFO_BYTES 72
#define SV_TEXDATA_BYTES 32
#define SV_FACE_BYTES 56

enum {
	SV_LUMP_ENTITIES = 0,
	SV_LUMP_PLANES = 1,
	SV_LUMP_VISIBILITY = 4,
	SV_LUMP_TEXDATA = 2,
	SV_LUMP_VERTEXES = 3,
	SV_LUMP_NODES = 5,
	SV_LUMP_LEAFFACES = 16,
	SV_LUMP_LEAFS = 10,
	SV_LUMP_FACES = 7,
	SV_LUMP_TEXINFO = 6,
	SV_LUMP_EDGES = 12,
	SV_LUMP_SURFEDGES = 13,
	SV_LUMP_LIGHTING = 8,
	SV_LUMP_TEXDATA_STRING_DATA = 43,
	SV_LUMP_TEXDATA_STRING_TABLE = 44
};

typedef struct {
	int32_t offset;
	int32_t length;
} svLump_t;

typedef struct {
	const byte *base;
	int size;
	svLump_t lumps[SV_LUMP_COUNT];
} svLoad_t;

static qboolean s_sourceVBSPLoaded;
static char s_sourceVBSPMap[MAX_QPATH];

#define SOURCE_ENTITY_MAX_LIGHTS 256
typedef struct {
	vec3_t origin;
	vec3_t color;
	float radius;
} sourceEntityLight_t;

static sourceEntityLight_t s_sourceEntityLights[SOURCE_ENTITY_MAX_LIGHTS];
static int s_sourceEntityLightCount;
static int s_sourceEntityCount;
static char s_sourceFGDPath[MAX_QPATH];
static int s_sourceFGDClasses;
static int s_sourceFGDKeys;

static uint32_t SV_U32( const byte *p ) {
	uint32_t v;
	Com_Memcpy( &v, p, sizeof( v ) );
	return LittleLong( v );
}

static int32_t SV_I32( const byte *p ) {
	return (int32_t)SV_U32( p );
}

static uint16_t SV_U16( const byte *p ) {
	uint16_t v;
	Com_Memcpy( &v, p, sizeof( v ) );
	return LittleShort( v );
}

static int16_t SV_I16( const byte *p ) {
	return (int16_t)SV_U16( p );
}

static float SV_F32( const byte *p ) {
	uint32_t bits = SV_U32( p );
	float value;
	Com_Memcpy( &value, &bits, sizeof( value ) );
	return value;
}

static const byte *SV_Lump( const svLoad_t *load, int index, int *length ) {
	if ( index < 0 || index >= SV_LUMP_COUNT || load->lumps[index].length <= 0 ) {
		*length = 0;
		return NULL;
	}
	*length = load->lumps[index].length;
	return load->base + load->lumps[index].offset;
}

static void SVBSP_Init( svLoad_t *load, const byte *buffer, int size, const char *mapname ) {
	int i;
	uint32_t version;

	if ( size < SV_HEADER_BYTES + SV_LUMP_COUNT * SV_LUMP_BYTES ||
			SV_U32( buffer ) != SV_MAGIC ) {
		ri.Error( ERR_DROP, "%s: %s is not a Source VBSP", __func__, mapname );
	}
	version = SV_U32( buffer + 4 );
	if ( version < 19 || version > 21 ) {
		ri.Error( ERR_DROP, "%s: %s has unsupported Source VBSP version %u", __func__, mapname, version );
	}
	load->base = buffer;
	load->size = size;
	for ( i = 0; i < SV_LUMP_COUNT; i++ ) {
		const byte *entry = buffer + SV_HEADER_BYTES + i * SV_LUMP_BYTES;
		int32_t offset = SV_I32( entry + 0 );
		int32_t length = SV_I32( entry + 4 );
		if ( offset < 0 || length < 0 || offset > size || length > size - offset ) {
			ri.Error( ERR_DROP, "%s: %s has invalid VBSP lump %d", __func__, mapname, i );
		}
		load->lumps[i].offset = offset;
		load->lumps[i].length = length;
	}
}

static int SV_Count( const svLoad_t *load, int lump, int stride, const char *what ) {
	int length;
	(void)SV_Lump( load, lump, &length );
	if ( stride <= 0 || length % stride ) {
		ri.Error( ERR_DROP, "Source VBSP: malformed %s lump", what );
	}
	return length / stride;
}

static qboolean SV_LoadVisibility( const svLoad_t *load, world_t *world ) {
	const byte *vis;
	byte *visOut;
	int length, clusters, rowBytes, i;

	vis = SV_Lump( load, SV_LUMP_VISIBILITY, &length );
	if ( !vis || length < SV_VIS_HEADER_BYTES ) return qfalse;
	clusters = SV_I32( vis );
	if ( clusters <= 0 || clusters > 65536 ||
		(uint64_t)SV_VIS_HEADER_BYTES + (uint64_t)clusters * 8 > (uint64_t)length ) return qfalse;
	rowBytes = (clusters + 7) >> 3;
	visOut = ri.Hunk_Alloc( clusters * rowBytes, h_low );
	world->vis = visOut;
	world->novis = ri.Hunk_Alloc( rowBytes, h_low );
	Com_Memset( world->novis, 0xff, rowBytes );
	for ( i = 0; i < clusters; ++i ) {
		int offset = SV_I32( vis + SV_VIS_HEADER_BYTES + i * 8 );
		byte *row = visOut + i * rowBytes;
		int written = 0;
		if ( offset < 0 ) {
			Com_Memset( row, 0xff, rowBytes );
			continue;
		}
		if ( offset >= length ) return qfalse;
		while ( written < rowBytes ) {
			byte value = vis[offset++];
			if ( value ) {
				row[written++] = value;
			} else {
				int count;
				if ( offset >= length ) return qfalse;
				count = vis[offset++];
				if ( count <= 0 || written + count > rowBytes ) return qfalse;
				Com_Memset( row + written, 0, count );
				written += count;
			}
		}
	}
	world->numClusters = clusters;
	world->clusterBytes = rowBytes;
	return qtrue;
}

static qboolean SV_LoadTree( const svLoad_t *load, world_t *world,
	int numPlanes, int numFaces, const vec3_t mins, const vec3_t maxs ) {
	const byte *nodes, *leaves, *leafFaces;
	int nodeLength, leafLength, leafFaceLength;
	int nodeCount, leafCount, leafFaceCount, i;
	msurface_t **marks;
	int markCount = 0;

	nodes = SV_Lump( load, SV_LUMP_NODES, &nodeLength );
	leaves = SV_Lump( load, SV_LUMP_LEAFS, &leafLength );
	leafFaces = SV_Lump( load, SV_LUMP_LEAFFACES, &leafFaceLength );
	if ( !nodes || !leaves || !leafFaces || nodeLength % SV_NODE_BYTES ||
		leafLength % SV_LEAF_BYTES || leafFaceLength % SV_LEAFFACE_BYTES ) return qfalse;
	nodeCount = nodeLength / SV_NODE_BYTES;
	leafCount = leafLength / SV_LEAF_BYTES;
	leafFaceCount = leafFaceLength / SV_LEAFFACE_BYTES;
	if ( nodeCount <= 0 || leafCount <= 0 || nodeCount > 262144 || leafCount > 262144 ) return qfalse;
	world->nodes = ri.Hunk_Alloc( (nodeCount + leafCount) * sizeof( *world->nodes ), h_low );
	Com_Memset( world->nodes, 0, (nodeCount + leafCount) * sizeof( *world->nodes ) );
	world->numnodes = nodeCount + leafCount;
	world->numDecisionNodes = nodeCount;
	marks = ri.Hunk_Alloc( MAX( leafFaceCount, 1 ) * sizeof( *marks ), h_low );
	world->marksurfaces = marks;
	world->nummarksurfaces = 0;
	for ( i = 0; i < nodeCount; ++i ) {
		mnode_t *node = &world->nodes[i];
		int plane = SV_I32( nodes + i * SV_NODE_BYTES );
		int childIndex;
		node->contents = CONTENTS_NODE;
		if ( plane < 0 || plane >= numPlanes ) return qfalse;
		node->plane = &world->planes[plane];
		for ( childIndex = 0; childIndex < 2; ++childIndex ) {
			int child = SV_I32( nodes + i * SV_NODE_BYTES + 4 + childIndex * 4 );
			if ( child >= 0 ) {
				if ( child >= nodeCount ) return qfalse;
				node->children[childIndex] = &world->nodes[child];
			} else {
				int leaf = -1 - child;
				if ( leaf < 0 || leaf >= leafCount ) return qfalse;
				node->children[childIndex] = &world->nodes[nodeCount + leaf];
			}
		}
		for ( childIndex = 0; childIndex < 3; ++childIndex ) {
			node->mins[childIndex] = (float)SV_I16( nodes + i * SV_NODE_BYTES + 12 + childIndex * 2 );
			node->maxs[childIndex] = (float)SV_I16( nodes + i * SV_NODE_BYTES + 18 + childIndex * 2 );
		}
	}
	for ( i = 0; i < leafCount; ++i ) {
		mnode_t *leaf = &world->nodes[nodeCount + i];
		int first = SV_U16( leaves + i * SV_LEAF_BYTES + 20 );
		int count = SV_U16( leaves + i * SV_LEAF_BYTES + 22 );
		int j;
		leaf->contents = 0;
		leaf->cluster = SV_I16( leaves + i * SV_LEAF_BYTES + 4 );
		leaf->area = SV_U16( leaves + i * SV_LEAF_BYTES + 6 );
		for ( j = 0; j < 3; ++j ) {
			leaf->mins[j] = (float)SV_I16( leaves + i * SV_LEAF_BYTES + 8 + j * 2 );
			leaf->maxs[j] = (float)SV_I16( leaves + i * SV_LEAF_BYTES + 14 + j * 2 );
		}
		if ( first < 0 || count < 0 || first > leafFaceCount - count ) return qfalse;
		leaf->firstmarksurface = marks + markCount;
		leaf->nummarksurfaces = 0;
		for ( j = 0; j < count; ++j ) {
			int face = SV_U16( leafFaces + (first + j) * SV_LEAFFACE_BYTES );
			if ( face < 0 || face >= numFaces || !world->surfaces[face].data ) continue;
			marks[markCount++] = &world->surfaces[face];
			++leaf->nummarksurfaces;
		}
	}
	world->nummarksurfaces = markCount;
	for ( i = 0; i < nodeCount; ++i ) {
		int child;
		for ( child = 0; child < 2; ++child ) world->nodes[i].children[child]->parent = &world->nodes[i];
	}
	for ( i = 0; i < nodeCount + leafCount; ++i ) world->nodes[i].visframe = 0;
	if ( !SV_LoadVisibility( load, world ) ) {
		world->numClusters = 0;
		world->clusterBytes = 0;
		world->vis = NULL;
		world->novis = ri.Hunk_Alloc( 1, h_low );
		world->novis[0] = 0xff;
	}
	(void)mins;
	(void)maxs;
	return qtrue;
}

static void SV_MaterialName( char *out, int outSize, const svLoad_t *load, int texinfoIndex ) {
	int texinfoLength, texdataLength, tableLength, stringLength;
	const byte *texinfos = SV_Lump( load, SV_LUMP_TEXINFO, &texinfoLength );
	const byte *texdata = SV_Lump( load, SV_LUMP_TEXDATA, &texdataLength );
	const byte *table = SV_Lump( load, SV_LUMP_TEXDATA_STRING_TABLE, &tableLength );
	const byte *strings = SV_Lump( load, SV_LUMP_TEXDATA_STRING_DATA, &stringLength );
	int texdataIndex, stringIndex, nameOffset;
	const char *name;
	int n;

	Q_strncpyz( out, "source/missing_material", outSize );
	if ( !texinfos || !texdata || !table || !strings ||
			texinfoIndex < 0 || texinfoIndex >= texinfoLength / SV_TEXINFO_BYTES ) return;
	texdataIndex = SV_I32( texinfos + texinfoIndex * SV_TEXINFO_BYTES + 68 );
	if ( texdataIndex < 0 || texdataIndex >= texdataLength / SV_TEXDATA_BYTES ) return;
	stringIndex = SV_I32( texdata + texdataIndex * SV_TEXDATA_BYTES + 12 );
	if ( stringIndex < 0 || stringIndex > ( tableLength - 4 ) / 4 ) return;
	nameOffset = SV_I32( table + stringIndex * 4 );
	if ( nameOffset < 0 || nameOffset >= stringLength ) return;
	name = (const char *)( strings + nameOffset );
	n = 0;
	while ( n < stringLength - nameOffset && name[n] && n < MAX_QPATH - 8 ) n++;
	if ( n <= 0 || n >= stringLength - nameOffset ) return;
	Com_sprintf( out, outSize, "source/%.*s", n, name );
}

static qboolean SV_Surface( const svLoad_t *load, int faceIndex,
		const byte *faces, const byte *vertices, const byte *planes,
		const byte *edges, const byte *surfedges, int numVertices, int numPlanes,
		int numEdges, int numSurfedges, shader_t *fallback, msurface_t *surface ) {
	const byte *in = faces + faceIndex * SV_FACE_BYTES;
	int planeIndex = (int)SV_U16( in + 0 );
	int firstEdge = SV_I32( in + 4 );
	int edgeCount = (int)SV_U16( in + 8 );
	/* dface_t: planenum(2), side/onNode(2), firstedge(4),
	 * numedges(2), texinfo(2). */
	int texinfoIndex = SV_I16( in + 10 );
	int j, indicesOffset, allocationSize;
	char material[MAX_QPATH];
	srfSurfaceFace_t *face;

	if ( edgeCount < 3 || edgeCount > 8192 || planeIndex < 0 || planeIndex >= numPlanes ||
			firstEdge < 0 || firstEdge > numSurfedges - edgeCount ) return qfalse;
	indicesOffset = sizeof( *face ) - sizeof( face->points ) +
			sizeof( face->points[0] ) * edgeCount;
	allocationSize = indicesOffset + sizeof( int ) * ( edgeCount - 2 ) * 3;
	face = ri.Hunk_Alloc( allocationSize, h_low );
	Com_Memset( face, 0, allocationSize );
	face->surfaceType = SF_FACE;
	face->numPoints = edgeCount;
	face->numIndices = ( edgeCount - 2 ) * 3;
	face->ofsIndices = indicesOffset;
	for ( j = 0; j < 3; j++ ) face->plane.normal[j] = SV_F32( planes + planeIndex * SV_PLANE_BYTES + j * 4 );
	face->plane.dist = SV_F32( planes + planeIndex * SV_PLANE_BYTES + 12 );
	if ( in[2] ) {
		VectorNegate( face->plane.normal, face->plane.normal );
		face->plane.dist = -face->plane.dist;
	}
	face->plane.type = PlaneTypeForNormal( face->plane.normal );
	SetPlaneSignbits( &face->plane );
	SV_MaterialName( material, sizeof( material ), load, texinfoIndex );
	surface->shader = R_FindShader( material, LIGHTMAP_BY_VERTEX, qtrue );
	if ( !surface->shader ) surface->shader = fallback;
	surface->fogIndex = 0;
	for ( j = 0; j < edgeCount; j++ ) {
		int surfedge = SV_I32( surfedges + ( firstEdge + j ) * SV_SURFEDGE_BYTES );
		int edgeIndex = surfedge < 0 ? -surfedge : surfedge;
		int vertexIndex;
		float *point = face->points[j];
		if ( edgeIndex < 0 || edgeIndex >= numEdges ) return qfalse;
		vertexIndex = surfedge >= 0 ? SV_U16( edges + edgeIndex * SV_EDGE_BYTES )
			: SV_U16( edges + edgeIndex * SV_EDGE_BYTES + 2 );
		if ( vertexIndex < 0 || vertexIndex >= numVertices ) return qfalse;
		point[0] = SV_F32( vertices + vertexIndex * SV_VERTEX_BYTES + 0 );
		point[1] = SV_F32( vertices + vertexIndex * SV_VERTEX_BYTES + 4 );
		point[2] = SV_F32( vertices + vertexIndex * SV_VERTEX_BYTES + 8 );
		point[3] = face->plane.normal[0]; point[4] = face->plane.normal[1]; point[5] = face->plane.normal[2];
		point[6] = point[0] * 0.01f; point[7] = point[1] * 0.01f;
		point[8] = point[9] = 0.0f;
		((byte *)&point[10])[0] = 255; ((byte *)&point[10])[1] = 255;
		((byte *)&point[10])[2] = 255; ((byte *)&point[10])[3] = 255;
	}
	for ( j = 0; j < edgeCount - 2; j++ ) {
		int *indices = (int *)( (byte *)face + face->ofsIndices );
		indices[j * 3 + 0] = 0; indices[j * 3 + 1] = j + 1; indices[j * 3 + 2] = j + 2;
	}
	vk_mikkt_bsp_face_generate( face );
	surface->data = (surfaceType_t *)face;
	return qtrue;
}

void R_LoadSourceVBSPWorld( const char *mapname, const byte *buffer, int size, world_t *world ) {
	svLoad_t load;
	const byte *entities, *planes, *vertices, *faces, *edges, *surfedges;
	int entityLength, planeLength, vertexLength, faceLength, edgeLength, surfedgeLength;
	int numPlanes, numVertices, numFaces, numEdges, numSurfedges, i, validFaces = 0;
	qboolean treeLoaded;
	vec3_t mins = { 1e30f, 1e30f, 1e30f }, maxs = { -1e30f, -1e30f, -1e30f };

	SVBSP_Init( &load, buffer, size, mapname );
	s_sourceVBSPLoaded = qfalse;
	Q_strncpyz( s_sourceVBSPMap, mapname, sizeof( s_sourceVBSPMap ) );
	planes = SV_Lump( &load, SV_LUMP_PLANES, &planeLength );
	vertices = SV_Lump( &load, SV_LUMP_VERTEXES, &vertexLength );
	faces = SV_Lump( &load, SV_LUMP_FACES, &faceLength );
	edges = SV_Lump( &load, SV_LUMP_EDGES, &edgeLength );
	surfedges = SV_Lump( &load, SV_LUMP_SURFEDGES, &surfedgeLength );
	numPlanes = SV_Count( &load, SV_LUMP_PLANES, SV_PLANE_BYTES, "planes" );
	numVertices = SV_Count( &load, SV_LUMP_VERTEXES, SV_VERTEX_BYTES, "vertices" );
	numFaces = SV_Count( &load, SV_LUMP_FACES, SV_FACE_BYTES, "faces" );
	numEdges = SV_Count( &load, SV_LUMP_EDGES, SV_EDGE_BYTES, "edges" );
	numSurfedges = SV_Count( &load, SV_LUMP_SURFEDGES, SV_SURFEDGE_BYTES, "surfedges" );
	if ( !planes || !vertices || !faces || !edges || !surfedges || numSurfedges < 3 )
		ri.Error( ERR_DROP, "%s: %s lacks required Source world geometry", __func__, mapname );
	(void)planeLength; (void)vertexLength; (void)faceLength; (void)edgeLength; (void)surfedgeLength;

	world->numplanes = numPlanes;
	world->planes = ri.Hunk_Alloc( MAX( numPlanes, 1 ) * sizeof( *world->planes ), h_low );
	for ( i = 0; i < numPlanes; i++ ) {
		cplane_t *p = &world->planes[i];
		p->normal[0] = SV_F32( planes + i * SV_PLANE_BYTES + 0 );
		p->normal[1] = SV_F32( planes + i * SV_PLANE_BYTES + 4 );
		p->normal[2] = SV_F32( planes + i * SV_PLANE_BYTES + 8 );
		p->dist = SV_F32( planes + i * SV_PLANE_BYTES + 12 );
		p->type = PlaneTypeForNormal( p->normal ); SetPlaneSignbits( p );
	}
	world->numsurfaces = numFaces;
	world->surfaces = ri.Hunk_Alloc( MAX( numFaces, 1 ) * sizeof( *world->surfaces ), h_low );
	Com_Memset( world->surfaces, 0, MAX( numFaces, 1 ) * sizeof( *world->surfaces ) );
	for ( i = 0; i < numFaces; i++ ) {
		if ( SV_Surface( &load, i, faces, vertices, planes, edges, surfedges,
				numVertices, numPlanes, numEdges, numSurfedges, tr.defaultShader, &world->surfaces[i] ) ) {
			validFaces++;
		}
	}
	world->nummarksurfaces = validFaces;
	world->marksurfaces = ri.Hunk_Alloc( MAX( validFaces, 1 ) * sizeof( *world->marksurfaces ), h_low );
	world->nummarksurfaces = 0;
	for ( i = 0; i < numFaces; i++ ) if ( world->surfaces[i].data ) world->marksurfaces[world->nummarksurfaces++] = &world->surfaces[i];

	world->numnodes = 1; world->numDecisionNodes = 0;
	world->nodes = ri.Hunk_Alloc( sizeof( *world->nodes ), h_low );
	Com_Memset( world->nodes, 0, sizeof( *world->nodes ) );
	world->nodes[0].contents = 0; world->nodes[0].cluster = -1;
	world->nodes[0].firstmarksurface = world->marksurfaces;
	world->nodes[0].nummarksurfaces = world->nummarksurfaces;
	for ( i = 0; i < numVertices; i++ ) {
		int j; const byte *v = vertices + i * SV_VERTEX_BYTES;
		for ( j = 0; j < 3; j++ ) { float value = SV_F32( v + j * 4 ); if ( value < mins[j] ) mins[j] = value; if ( value > maxs[j] ) maxs[j] = value; }
	}
	VectorCopy( mins, world->nodes[0].mins ); VectorCopy( maxs, world->nodes[0].maxs );
	world->numClusters = 0; world->clusterBytes = 0;
	world->novis = ri.Hunk_Alloc( 1, h_low ); world->novis[0] = 0xff; world->vis = NULL;
	entities = SV_Lump( &load, SV_LUMP_ENTITIES, &entityLength );
	world->entityString = ri.Hunk_Alloc( entityLength + 1, h_low );
	if ( entities && entityLength ) Com_Memcpy( world->entityString, entities, entityLength );
	world->entityString[entityLength] = '\0';
	R_SourceEntities_LoadEntityString( world->entityString );
	world->numBModels = 0; world->bmodels = NULL; world->numfogs = 0; world->fogs = NULL;
	treeLoaded = SV_LoadTree( &load, world, numPlanes, numFaces, mins, maxs );
	ri.Printf( PRINT_ALL, "...Source VBSP: version %u, %d/%d world faces, %s visibility\n",
		SV_U32( buffer + 4 ), validFaces, numFaces,
		treeLoaded && world->numClusters > 0 ? "Source PVS" : "all-visible fallback" );
	s_sourceVBSPLoaded = validFaces > 0 ? qtrue : qfalse;
}

void R_SourceVBSP_Clear( void ) {
	s_sourceVBSPLoaded = qfalse;
	s_sourceVBSPMap[0] = '\0';
}

void R_SourceVBSP_Status_f( void ) {
	if ( !s_sourceVBSPLoaded || !tr.world ) {
		ri.Printf( PRINT_ALL, "Source VBSP: inactive\n" );
		return;
	}
	ri.Printf( PRINT_ALL, "Source VBSP: active map=%s surfaces=%d marksurfaces=%d planes=%d nodes=%d clusters=%d visibility=%s entities=%s\n",
		s_sourceVBSPMap, tr.world->numsurfaces, tr.world->nummarksurfaces,
		tr.world->numplanes, tr.world->numDecisionNodes, tr.world->numClusters,
		tr.world->vis ? "PVS" : "all-visible", tr.world->entityString ? "loaded" : "none" );
}

static const char *SE_Token( const char *p, char *out, int outSize ) {
	int n = 0;
	while ( p && *p && (unsigned char)*p <= ' ' ) ++p;
	if ( !p || !*p ) return NULL;
	if ( *p == '{' || *p == '}' ) {
		out[0] = *p;
		out[1] = '\0';
		return p + 1;
	}
	if ( *p == '"' ) {
		++p;
		while ( *p && *p != '"' ) {
			if ( n + 1 < outSize ) out[n++] = *p;
			++p;
		}
		if ( *p == '"' ) ++p;
	} else {
		while ( *p && (unsigned char)*p > ' ' && *p != '{' && *p != '}' ) {
			if ( n + 1 < outSize ) out[n++] = *p;
			++p;
		}
	}
	out[n] = '\0';
	return p;
}

static void SE_ParseLight( const char *classname, const char *originText,
	const char *lightText, const char *distanceText ) {
	float x, y, z, brightness = 300.0f;
	float r = 1.0f, g = 1.0f, b = 1.0f;
	float distance = 300.0f;
	if ( Q_stricmp( classname, "light" ) && Q_stricmp( classname, "light_spot" ) &&
		Q_stricmp( classname, "light_environment" ) ) return;
	if ( !originText || sscanf( originText, "%f %f %f", &x, &y, &z ) != 3 ) return;
	if ( lightText ) {
		float cr, cg, cb, value;
		if ( sscanf( lightText, "%f %f %f %f", &cr, &cg, &cb, &value ) >= 3 ) {
			r = cr / 255.0f; g = cg / 255.0f; b = cb / 255.0f;
			if ( sscanf( lightText, "%f %f %f %f", &cr, &cg, &cb, &value ) == 4 ) brightness = value;
		}
	}
	if ( distanceText ) sscanf( distanceText, "%f", &distance );
	if ( distance > 0.0f ) brightness = distance;
	if ( s_sourceEntityLightCount >= SOURCE_ENTITY_MAX_LIGHTS ) return;
	VectorSet( s_sourceEntityLights[s_sourceEntityLightCount].origin, x, y, z );
	VectorSet( s_sourceEntityLights[s_sourceEntityLightCount].color,
		Com_Clamp( 0.0f, 1.0f, r ), Com_Clamp( 0.0f, 1.0f, g ), Com_Clamp( 0.0f, 1.0f, b ) );
	s_sourceEntityLights[s_sourceEntityLightCount].radius = Com_Clamp( 16.0f, 4096.0f, brightness );
	++s_sourceEntityLightCount;
}

void R_SourceEntities_LoadEntityString( const char *entityString ) {
	const char *p = entityString;
	char token[256], key[128], value[512];
	if ( !entityString ) return;
	s_sourceEntityLightCount = 0;
	s_sourceEntityCount = 0;
	while ( ( p = SE_Token( p, token, sizeof( token ) ) ) != NULL ) {
		char classname[128] = "", origin[128] = "", light[256] = "", distance[64] = "";
		if ( Q_stricmp( token, "{" ) ) continue;
		++s_sourceEntityCount;
		while ( ( p = SE_Token( p, key, sizeof( key ) ) ) != NULL && Q_stricmp( key, "}" ) ) {
			p = SE_Token( p, value, sizeof( value ) );
			if ( !p ) break;
			if ( !Q_stricmp( key, "classname" ) ) Q_strncpyz( classname, value, sizeof( classname ) );
			else if ( !Q_stricmp( key, "origin" ) ) Q_strncpyz( origin, value, sizeof( origin ) );
			else if ( !Q_stricmp( key, "_light" ) || !Q_stricmp( key, "_lightHDR" ) ) Q_strncpyz( light, value, sizeof( light ) );
			else if ( !Q_stricmp( key, "_distance" ) || !Q_stricmp( key, "distance" ) ) Q_strncpyz( distance, value, sizeof( distance ) );
		}
		SE_ParseLight( classname, origin, light[0] ? light : NULL, distance[0] ? distance : NULL );
	}
}

void R_SourceEntities_Clear( void ) {
	s_sourceEntityLightCount = 0;
	s_sourceEntityCount = 0;
}

void R_SourceEntities_AddLights( void ) {
	int i;
	if ( !r_sourceEntities || !r_sourceEntities->integer || !s_sourceVBSPLoaded ||
		(tr.refdef.rdflags & RDF_NOWORLDMODEL) ) return;
	for ( i = 0; i < s_sourceEntityLightCount; ++i ) {
		RE_AddLightToScene( s_sourceEntityLights[i].origin, s_sourceEntityLights[i].radius,
			s_sourceEntityLights[i].color[0], s_sourceEntityLights[i].color[1], s_sourceEntityLights[i].color[2] );
	}
}

void R_SourceEntities_LoadFGD_f( void ) {
	void *buffer = NULL;
	const char *path = Cmd_Argv( 1 );
	int length, i;
	char *text;
	if ( !path || !path[0] ) {
		ri.Printf( PRINT_ALL, "usage: source_fgd_load <path.fgd>\n" );
		return;
	}
	length = ri.FS_ReadFile( path, &buffer );
	if ( length <= 0 || !buffer ) {
		ri.Printf( PRINT_WARNING, "source_fgd_load: couldn't read %s\n", path );
		return;
	}
	text = (char *)ri.Malloc( length + 1 );
	if ( !text ) { ri.FS_FreeFile( buffer ); return; }
	Com_Memcpy( text, buffer, length );
	text[length] = '\0';
	s_sourceFGDClasses = 0;
	s_sourceFGDKeys = 0;
	for ( i = 0; i < length; ++i ) {
		if ( text[i] == '@' && !Q_stricmpn( text + i, "@PointClass", 11 ) ) ++s_sourceFGDClasses;
		if ( text[i] == '@' && !Q_stricmpn( text + i, "@SolidClass", 11 ) ) ++s_sourceFGDClasses;
		if ( text[i] == ':' && i > 0 && text[i - 1] != ':' ) ++s_sourceFGDKeys;
	}
	Q_strncpyz( s_sourceFGDPath, path, sizeof( s_sourceFGDPath ) );
	ri.Free( text );
	ri.FS_FreeFile( buffer );
	ri.Printf( PRINT_ALL, "Source FGD: loaded %s classes=%d key declarations=%d\n",
		s_sourceFGDPath, s_sourceFGDClasses, s_sourceFGDKeys );
}

void R_SourceEntities_FGDStatus_f( void ) {
	ri.Printf( PRINT_ALL, "Source FGD: %s classes=%d key declarations=%d; entities=%d lights=%d\n",
		s_sourceFGDPath[0] ? s_sourceFGDPath : "inactive", s_sourceFGDClasses,
		s_sourceFGDKeys, s_sourceEntityCount, s_sourceEntityLightCount );
}
