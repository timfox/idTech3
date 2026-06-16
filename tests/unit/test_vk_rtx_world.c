/*
 * Unit tests: Vulkan RTX world BLAS geometry extraction.
 * Exercises deterministic CPU-side packing without requiring Vulkan hardware.
 */
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "renderers/vulkan/vk_rtx_world.h"

#define ASSERT(cond, msg) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

typedef struct {
	srfSurfaceFace_t face;
	float extraPoints[3][VERTEXSIZE];
	unsigned indices[6];
} face_fixture_t;

static void set_point( float points[][VERTEXSIZE], int index, float x, float y, float z )
{
	memset( points[index], 0, sizeof( points[index] ) );
	points[index][0] = x;
	points[index][1] = y;
	points[index][2] = z;
}

static void init_face_fixture( face_fixture_t *fixture )
{
	float ( *points )[VERTEXSIZE];

	memset( fixture, 0, sizeof( *fixture ) );
	fixture->face.surfaceType = SF_FACE;
	fixture->face.numPoints = 4;
	fixture->face.numIndices = 6;
	fixture->face.ofsIndices = (int)offsetof( face_fixture_t, indices );
	fixture->indices[0] = 0;
	fixture->indices[1] = 1;
	fixture->indices[2] = 2;
	fixture->indices[3] = 0;
	fixture->indices[4] = 2;
	fixture->indices[5] = 3;

	points = fixture->face.points;
	set_point( points, 0, 10.0f, 11.0f, 12.0f );
	set_point( points, 1, 20.0f, 21.0f, 22.0f );
	set_point( points, 2, 30.0f, 31.0f, 32.0f );
	set_point( points, 3, 40.0f, 41.0f, 42.0f );
}

static void set_vert( srfVert_t *vert, float x, float y, float z )
{
	memset( vert, 0, sizeof( *vert ) );
	vert->xyz[0] = x;
	vert->xyz[1] = y;
	vert->xyz[2] = z;
}

static void init_triangles_fixture( srfTriangles_t *tri, srfVert_t verts[3], int indexes[3] )
{
	memset( tri, 0, sizeof( *tri ) );
	tri->surfaceType = SF_TRIANGLES;
	tri->numIndexes = 3;
	tri->indexes = indexes;
	tri->numVerts = 3;
	tri->verts = verts;
	indexes[0] = 2;
	indexes[1] = 1;
	indexes[2] = 0;
	set_vert( &verts[0], 100.0f, 101.0f, 102.0f );
	set_vert( &verts[1], 110.0f, 111.0f, 112.0f );
	set_vert( &verts[2], 120.0f, 121.0f, 122.0f );
}

static void init_world( world_t *world, bmodel_t *bmodels, msurface_t *surfaces, int numSurfaces )
{
	memset( world, 0, sizeof( *world ) );
	memset( bmodels, 0, sizeof( *bmodels ) );
	world->bmodels = bmodels;
	world->numBModels = 1;
	world->surfaces = surfaces;
	world->numsurfaces = numSurfaces;
	bmodels[0].firstSurface = surfaces;
	bmodels[0].numSurfaces = numSurfaces;
}

static int test_count_skips_invalid_inputs( void )
{
	world_t world;
	bmodel_t bmodel;
	msurface_t surfaces[2];
	face_fixture_t face;
	surfaceType_t skippedType = SF_GRID;

	init_face_fixture( &face );
	memset( surfaces, 0, sizeof( surfaces ) );
	surfaces[0].data = NULL;
	surfaces[1].data = &skippedType;
	init_world( &world, &bmodel, surfaces, 2 );

	ASSERT( vk_rtx_world_count_primitives( NULL, 8u ) == 0u, "NULL world has no primitives" );
	ASSERT( vk_rtx_world_pack( NULL, 8u, NULL, NULL ) == 0u, "NULL world packs no primitives" );
	ASSERT( vk_rtx_world_count_primitives( &world, 8u ) == 0u, "unsupported/null surfaces are skipped" );

	surfaces[0].data = &face.face.surfaceType;
	face.face.numIndices = 5;
	ASSERT( vk_rtx_world_count_primitives( &world, 8u ) == 0u, "malformed face index count is skipped" );
	return 0;
}

static int test_count_honors_primitive_cap( void )
{
	world_t world;
	bmodel_t bmodel;
	msurface_t surfaces[1];
	face_fixture_t face;

	init_face_fixture( &face );
	memset( surfaces, 0, sizeof( surfaces ) );
	surfaces[0].data = &face.face.surfaceType;
	init_world( &world, &bmodel, surfaces, 1 );

	ASSERT( vk_rtx_world_count_primitives( &world, 8u ) == 2u, "face fixture has two triangles" );
	ASSERT( vk_rtx_world_count_primitives( &world, 1u ) == 1u, "primitive count is capped" );
	return 0;
}

static int test_pack_face_and_triangles( void )
{
	world_t world;
	bmodel_t bmodel;
	msurface_t surfaces[2];
	face_fixture_t face;
	srfTriangles_t tri;
	srfVert_t verts[3];
	int triIndexes[3];
	float positions[9 * 3];
	uint32_t indices[9];

	init_face_fixture( &face );
	init_triangles_fixture( &tri, verts, triIndexes );
	memset( surfaces, 0, sizeof( surfaces ) );
	surfaces[0].data = &face.face.surfaceType;
	surfaces[1].data = &tri.surfaceType;
	init_world( &world, &bmodel, surfaces, 2 );
	memset( positions, 0, sizeof( positions ) );
	memset( indices, 0, sizeof( indices ) );

	ASSERT( vk_rtx_world_count_primitives( &world, 8u ) == 3u, "face plus trisoup primitive count" );
	ASSERT( vk_rtx_world_pack( &world, 8u, positions, indices ) == 3u, "face plus trisoup packed count" );

	ASSERT( positions[0] == 10.0f && positions[1] == 11.0f && positions[2] == 12.0f, "face tri 0 vertex 0" );
	ASSERT( positions[3] == 20.0f && positions[4] == 21.0f && positions[5] == 22.0f, "face tri 0 vertex 1" );
	ASSERT( positions[6] == 30.0f && positions[7] == 31.0f && positions[8] == 32.0f, "face tri 0 vertex 2" );
	ASSERT( indices[0] == 0u && indices[1] == 1u && indices[2] == 2u, "face tri 0 indices" );

	ASSERT( positions[9] == 10.0f && positions[10] == 11.0f && positions[11] == 12.0f, "face tri 1 reuses face point 0" );
	ASSERT( positions[12] == 30.0f && positions[13] == 31.0f && positions[14] == 32.0f, "face tri 1 reuses face point 2" );
	ASSERT( positions[15] == 40.0f && positions[16] == 41.0f && positions[17] == 42.0f, "face tri 1 uses face point 3" );
	ASSERT( positions[18] == 120.0f && positions[19] == 121.0f && positions[20] == 122.0f, "trisoup honors source index order" );
	ASSERT( indices[6] == 6u && indices[7] == 7u && indices[8] == 8u, "trisoup indices continue after face vertices" );
	return 0;
}

static int test_pack_stops_at_cap( void )
{
	world_t world;
	bmodel_t bmodel;
	msurface_t surfaces[1];
	face_fixture_t face;
	float positions[3 * 3];
	uint32_t indices[3];

	init_face_fixture( &face );
	memset( surfaces, 0, sizeof( surfaces ) );
	surfaces[0].data = &face.face.surfaceType;
	init_world( &world, &bmodel, surfaces, 1 );
	memset( positions, 0, sizeof( positions ) );
	memset( indices, 0, sizeof( indices ) );

	ASSERT( vk_rtx_world_pack( &world, 1u, positions, indices ) == 1u, "packing stops at primitive cap" );
	ASSERT( positions[0] == 10.0f && positions[6] == 30.0f, "first face triangle packed under cap" );
	ASSERT( indices[0] == 0u && indices[1] == 1u && indices[2] == 2u, "capped face indices" );
	return 0;
}

int main( void )
{
	if ( test_count_skips_invalid_inputs() ) return 1;
	if ( test_count_honors_primitive_cap() ) return 1;
	if ( test_pack_face_and_triangles() ) return 1;
	if ( test_pack_stops_at_cap() ) return 1;

	printf( "PASS: unit_vk_rtx_world\n" );
	return 0;
}
