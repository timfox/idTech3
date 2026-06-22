/*
 * Arc Blanc Vulkan path: height texture upload + tessellated ocean draw.
 */
#include "../../tr_local.h"
#include "../../tr_common.h"
#include "vk_arc_blanc.h"
#include "vk_arc_blanc_gpu.h"

static image_t *s_arcBlancImage = NULL;
static cvar_t *r_arcBlanc;
static cvar_t *r_arcBlancDraw;
static cvar_t *r_arcBlancMeshDiv;
static cvar_t *r_arcBlancTile;
static cvar_t *r_arcBlancSeaLevel;
static shader_t *s_arcBlancShader = NULL;
static qboolean s_arcBlancLogged = qfalse;

#define AB_OCEAN_MAX_DIV 128

static void R_ArcBlanc_EnsureShader( void )
{
	if ( s_arcBlancShader ) {
		return;
	}
	s_arcBlancShader = R_FindShader( "arc_blanc_ocean", LIGHTMAP_NONE, qtrue );
	if ( !s_arcBlancShader || s_arcBlancShader->defaultShader ) {
		s_arcBlancShader = tr.defaultShader;
	}
}

void R_ArcBlanc_Init( void )
{
	r_arcBlanc = ri.Cvar_Get( "r_arcBlanc", "0", CVAR_ARCHIVE );
	r_arcBlancDraw = ri.Cvar_Get( "r_arcBlancDraw", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_arcBlancDraw,
		"Draw tessellated Arc Blanc ocean mesh (requires r_arcBlanc 1)." );
	r_arcBlancMeshDiv = ri.Cvar_Get( "r_arcBlancMeshDiv", "48", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_arcBlancMeshDiv, "8", "128", CV_INTEGER );
	ri.Cvar_SetDescription( r_arcBlancMeshDiv, "Ocean patch subdivisions per tile edge (8-128)." );
	r_arcBlancTile = ri.Cvar_Get( "r_arcBlancTile", "256", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_arcBlancTile, "Ocean tile size for renderer mesh snapping." );
	r_arcBlancSeaLevel = ri.Cvar_Get( "r_arcBlancSeaLevel", "0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_arcBlancSeaLevel, "Base sea level offset added to sampled heights." );
	VK_ArcBlancGpu_Init();
}

void VK_ArcBlanc_Shutdown( void )
{
	VK_ArcBlancGpu_Shutdown();
	s_arcBlancImage = NULL;
	s_arcBlancShader = NULL;
	s_arcBlancLogged = qfalse;
}

void RE_ArcBlancUploadHeightMap( const byte *rgba, int width, int height )
{
	if ( !rgba || width < 1 || height < 1 ) {
		return;
	}
	if ( !s_arcBlancImage || s_arcBlancImage->width != width || s_arcBlancImage->height != height ) {
		s_arcBlancImage = R_CreateImage( "*arc_blanc_height", NULL, NULL, width, height,
			IMGFLAG_CLAMPTOEDGE | IMGFLAG_NOLIGHTSCALE, 0, 0 );
	}
	if ( s_arcBlancImage ) {
		R_UploadSubImage( (byte *)rgba, 0, 0, width, height, s_arcBlancImage );
	}
}

static float R_ArcBlanc_SampleHeight( float x, float z )
{
	if ( ri.ArcBlancSampleHeight ) {
		return ri.ArcBlancSampleHeight( x, z );
	}
	return r_arcBlancSeaLevel ? r_arcBlancSeaLevel->value : 0.0f;
}

static void R_ArcBlanc_ComputeNormal( float x, float z, float eps, vec3_t out )
{
	const float hL = R_ArcBlanc_SampleHeight( x - eps, z );
	const float hR = R_ArcBlanc_SampleHeight( x + eps, z );
	const float hD = R_ArcBlanc_SampleHeight( x, z - eps );
	const float hU = R_ArcBlanc_SampleHeight( x, z + eps );
	out[0] = hL - hR;
	out[1] = 2.0f * eps;
	out[2] = hD - hU;
	VectorNormalize( out );
}

static surfaceType_t *R_ArcBlanc_BuildPatch( int div, float tileSize, float originX, float originZ )
{
	srfTriangles_t *tri;
	int vertsPerSide = div + 1;
	int numVerts = vertsPerSide * vertsPerSide;
	int numIndexes = div * div * 6;
	int allocSize;
	int w, h, i;
	float step = tileSize / (float)div;
	float eps = step * 0.5f;

	allocSize = (int)( sizeof( *tri ) + numVerts * sizeof( tri->verts[0] ) +
		numIndexes * sizeof( tri->indexes[0] ) );
	tri = ri.Hunk_Alloc( allocSize, h_low );
	tri->surfaceType = SF_TRIANGLES;
	tri->dlightBits = 0;
#ifdef USE_VBO
	tri->vboItemIndex = 0;
#endif
	tri->numVerts = numVerts;
	tri->numIndexes = numIndexes;
	tri->verts = (srfVert_t *)( tri + 1 );
	tri->indexes = (int *)( tri->verts + tri->numVerts );

	ClearBounds( tri->bounds[0], tri->bounds[1] );
	VectorSet( tri->localOrigin, originX + tileSize * 0.5f, 0.0f, originZ + tileSize * 0.5f );
	tri->radius = tileSize * 0.75f;

	i = 0;
	for ( h = 0; h < vertsPerSide; h++ ) {
		for ( w = 0; w < vertsPerSide; w++ ) {
			srfVert_t *vert = &tri->verts[i];
			const float x = originX + (float)w * step;
			const float z = originZ + (float)h * step;
			const float y = R_ArcBlanc_SampleHeight( x, z );
			byte depthColor;

			vert->xyz[0] = x;
			vert->xyz[1] = y;
			vert->xyz[2] = z;
			vert->st[0] = (float)w / (float)div;
			vert->st[1] = (float)h / (float)div;
			vert->lightmap[0] = 0.0f;
			vert->lightmap[1] = 0.0f;
			R_ArcBlanc_ComputeNormal( x, z, eps, vert->normal );
			depthColor = (byte)Com_Clamp( 0, 255, 140 + (int)( y * 4.0f ) );
			vert->color.rgba[0] = 20;
			vert->color.rgba[1] = 90;
			vert->color.rgba[2] = depthColor;
			vert->color.rgba[3] = 220;
			AddPointToBounds( vert->xyz, tri->bounds[0], tri->bounds[1] );
			i++;
		}
	}

	i = 0;
	for ( h = 0; h < div; h++ ) {
		for ( w = 0; w < div; w++ ) {
			const int v1 = h * vertsPerSide + w + 1;
			const int v2 = v1 - 1;
			const int v3 = v2 + vertsPerSide;
			const int v4 = v3 + 1;

			tri->indexes[i++] = v2;
			tri->indexes[i++] = v3;
			tri->indexes[i++] = v1;
			tri->indexes[i++] = v1;
			tri->indexes[i++] = v3;
			tri->indexes[i++] = v4;
		}
	}

	return (surfaceType_t *)tri;
}

void R_ArcBlanc_AddSurfaces( void )
{
	float tileSize;
	int div;
	float originX, originZ;
	surfaceType_t *surf;
	int tiles = 1;
	int tx, tz;

	if ( !r_arcBlanc || !r_arcBlanc->integer ) {
		return;
	}
	if ( !r_arcBlancDraw || !r_arcBlancDraw->integer ) {
		return;
	}
	if ( !ri.ArcBlancSampleHeight ) {
		return;
	}
	if ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) {
		return;
	}

	if ( !s_arcBlancLogged ) {
		s_arcBlancLogged = qtrue;
		ri.Printf( PRINT_ALL, "[arc_blanc] renderer draw path active (mesh upload + tessellated surface)\n" );
	}

	R_ArcBlanc_EnsureShader();
	tileSize = r_arcBlancTile ? r_arcBlancTile->value : 256.0f;
	if ( tileSize < 32.0f ) {
		tileSize = 32.0f;
	}
	div = r_arcBlancMeshDiv ? r_arcBlancMeshDiv->integer : 48;
	if ( div < 8 ) {
		div = 8;
	}
	if ( div > AB_OCEAN_MAX_DIV ) {
		div = AB_OCEAN_MAX_DIV;
	}

	originX = floorf( tr.viewParms.or.origin[0] / tileSize ) * tileSize;
	originZ = floorf( tr.viewParms.or.origin[2] / tileSize ) * tileSize;

	tr.currentEntityNum = REFENTITYNUM_WORLD;
	tr.shiftedEntityNum = tr.currentEntityNum << QSORT_REFENTITYNUM_SHIFT;

	for ( tz = -tiles; tz <= tiles; tz++ ) {
		for ( tx = -tiles; tx <= tiles; tx++ ) {
			const float ox = originX + (float)tx * tileSize;
			const float oz = originZ + (float)tz * tileSize;
			vec3_t bounds[2];

			VectorSet( bounds[0], ox, -4096.0f, oz );
			VectorSet( bounds[1], ox + tileSize, 4096.0f, oz + tileSize );
			if ( R_CullLocalBox( bounds ) == CULL_OUT ) {
				continue;
			}

			surf = R_ArcBlanc_BuildPatch( div, tileSize, ox, oz );
			if ( surf ) {
				R_AddDrawSurf( surf, s_arcBlancShader, 0, 0 );
			}
		}
	}
}
