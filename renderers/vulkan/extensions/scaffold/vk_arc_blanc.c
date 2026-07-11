/*
 * Arc Blanc Vulkan path: height texture upload + tessellated ocean draw.
 * Chocolate layer: always linked; real path requires USE_ARC_BLANC=ON.
 */
#include "../../tr_local.h"
#include "../../tr_common.h"
#include "vk_arc_blanc.h"
#include "vk_arc_blanc_gpu.h"
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef USE_ARC_BLANC

static image_t *s_arcBlancImage = NULL;
static cvar_t *r_arcBlanc;
static cvar_t *r_arcBlancDraw;
static cvar_t *r_arcBlancMeshDiv;
static cvar_t *r_arcBlancTile;
static cvar_t *r_arcBlancSeaLevel;
static cvar_t *r_arcBlancTileRadius;
static cvar_t *r_arcBlancFollowCamera;
static cvar_t *r_arcBlancAdaptiveMesh;
static cvar_t *r_arcBlancMeshDivFar;
static cvar_t *r_arcBlancAdaptiveHeightStart;
static cvar_t *r_arcBlancAdaptiveHeightEnd;
static cvar_t *r_arcBlancNormalStrength;
static cvar_t *r_arcBlancFoam;
static cvar_t *r_arcBlancFoamIntensity;
static cvar_t *r_arcBlancFoamThreshold;
static cvar_t *r_arcBlancFoamSoftness;
static cvar_t *r_arcBlancTileBreak;
static cvar_t *r_arcBlancTileBreakOffset;
static cvar_t *r_arcBlancTileBreakBlend;
static cvar_t *r_arcBlancTileBreakCell;
static cvar_t *r_arcBlancLakeMode;
static cvar_t *r_arcBlancLakeCenter;
static cvar_t *r_arcBlancLakeExtents;
static cvar_t *r_arcBlancLakeAngle;
static shader_t *s_arcBlancShader = NULL;
static qboolean s_arcBlancLogged = qfalse;

#define AB_OCEAN_MAX_DIV 128

static float ab_clampf01( float v )
{
	if ( v < 0.0f ) {
		return 0.0f;
	}
	if ( v > 1.0f ) {
		return 1.0f;
	}
	return v;
}

static float ab_smoothstep( float edge0, float edge1, float x )
{
	float t;
	if ( edge1 <= edge0 ) {
		return x >= edge1 ? 1.0f : 0.0f;
	}
	t = ab_clampf01( ( x - edge0 ) / ( edge1 - edge0 ) );
	return t * t * ( 3.0f - 2.0f * t );
}

static float ab_hash2i( int x, int z )
{
	float s = sinf( (float)x * 127.1f + (float)z * 311.7f ) * 43758.5453f;
	return s - floorf( s );
}

static float ab_value_noise2( float x, float z )
{
	int ix = (int)floorf( x );
	int iz = (int)floorf( z );
	float fx = x - (float)ix;
	float fz = z - (float)iz;
	float n00 = ab_hash2i( ix, iz );
	float n10 = ab_hash2i( ix + 1, iz );
	float n01 = ab_hash2i( ix, iz + 1 );
	float n11 = ab_hash2i( ix + 1, iz + 1 );
	float sx = fx * fx * ( 3.0f - 2.0f * fx );
	float sz = fz * fz * ( 3.0f - 2.0f * fz );
	float nx0 = n00 + ( n10 - n00 ) * sx;
	float nx1 = n01 + ( n11 - n01 ) * sx;
	return nx0 + ( nx1 - nx0 ) * sz;
}

static qboolean ab_parse_vec3( const char *s, vec3_t out )
{
	float x, y, z;
	if ( !s || sscanf( s, "%f %f %f", &x, &y, &z ) != 3 ) {
		return qfalse;
	}
	out[0] = x;
	out[1] = y;
	out[2] = z;
	return qtrue;
}

static qboolean ab_parse_vec2( const char *s, float *x, float *z )
{
	return s && x && z && sscanf( s, "%f %f", x, z ) == 2;
}

static int R_ArcBlanc_AdaptiveDiv( int baseDiv, int farDiv, int tx, int tz, int tileRadius, float seaLevel )
{
	float ringT = 0.0f;
	float heightT = 0.0f;
	int ring = abs( tx ) > abs( tz ) ? abs( tx ) : abs( tz );
	float heightAboveSea = fabsf( tr.viewParms.or.origin[1] - seaLevel );
	float startHeight = r_arcBlancAdaptiveHeightStart ? r_arcBlancAdaptiveHeightStart->value : 512.0f;
	float endHeight = r_arcBlancAdaptiveHeightEnd ? r_arcBlancAdaptiveHeightEnd->value : 4096.0f;
	float t;

	if ( tileRadius > 0 ) {
		ringT = (float)ring / (float)tileRadius;
		if ( ringT > 1.0f ) {
			ringT = 1.0f;
		}
	}
	if ( endHeight > startHeight && heightAboveSea > startHeight ) {
		heightT = ( heightAboveSea - startHeight ) / ( endHeight - startHeight );
		if ( heightT > 1.0f ) {
			heightT = 1.0f;
		}
	}
	t = ringT + ( 1.0f - ringT ) * heightT * 0.6f;
	if ( t > 1.0f ) {
		t = 1.0f;
	}
	baseDiv = (int)floorf( (float)baseDiv + (float)( farDiv - baseDiv ) * t + 0.5f );
	if ( baseDiv < 8 ) {
		baseDiv = 8;
	}
	if ( baseDiv > AB_OCEAN_MAX_DIV ) {
		baseDiv = AB_OCEAN_MAX_DIV;
	}
	return baseDiv;
}

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
	ri.Cvar_SetDescription( r_arcBlanc,
		"Arc Blanc ocean (chocolate; requires USE_ARC_BLANC build). 0=off, 1=on." );
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
	r_arcBlancTileRadius = ri.Cvar_Get( "r_arcBlancTileRadius", "1", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_arcBlancTileRadius, "0", "6", CV_INTEGER );
	ri.Cvar_SetDescription( r_arcBlancTileRadius, "How many tiles around the camera the renderer draws." );
	r_arcBlancFollowCamera = ri.Cvar_Get( "r_arcBlancFollowCamera", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_arcBlancFollowCamera, "View-follow ocean placement. Disable to anchor the draw grid in world space." );
	r_arcBlancAdaptiveMesh = ri.Cvar_Get( "r_arcBlancAdaptiveMesh", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_arcBlancAdaptiveMesh, "Use ring- and height-based adaptive mesh density for Arc Blanc patches." );
	r_arcBlancMeshDivFar = ri.Cvar_Get( "r_arcBlancMeshDivFar", "20", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_arcBlancMeshDivFar, "8", "128", CV_INTEGER );
	ri.Cvar_SetDescription( r_arcBlancMeshDivFar, "Outer-ring patch subdivisions when adaptive mesh is enabled." );
	r_arcBlancAdaptiveHeightStart = ri.Cvar_Get( "r_arcBlancAdaptiveHeightStart", "512", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_arcBlancAdaptiveHeightStart, "Camera height above sea level where adaptive mesh starts reducing detail." );
	r_arcBlancAdaptiveHeightEnd = ri.Cvar_Get( "r_arcBlancAdaptiveHeightEnd", "4096", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_arcBlancAdaptiveHeightEnd, "Camera height above sea level where adaptive mesh reaches its full reduction." );
	r_arcBlancNormalStrength = ri.Cvar_Get( "r_arcBlancNormalStrength", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_arcBlancNormalStrength, "Renderer normal exaggeration for Arc Blanc shading." );
	r_arcBlancFoam = ri.Cvar_Get( "r_arcBlancFoam", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_arcBlancFoam, "Enable crest foam shading based on wave steepness." );
	r_arcBlancFoamIntensity = ri.Cvar_Get( "r_arcBlancFoamIntensity", "0.35", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_arcBlancFoamIntensity, "Foam visibility multiplier." );
	r_arcBlancFoamThreshold = ri.Cvar_Get( "r_arcBlancFoamThreshold", "0.28", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_arcBlancFoamThreshold, "Steepness threshold before crest foam appears." );
	r_arcBlancFoamSoftness = ri.Cvar_Get( "r_arcBlancFoamSoftness", "1.5", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_arcBlancFoamSoftness, "Foam edge shaping. Higher values soften and spread the foam." );
	r_arcBlancTileBreak = ri.Cvar_Get( "r_arcBlancTileBreak", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_arcBlancTileBreak, "Blend a second offset sample to reduce obvious ocean tiling." );
	r_arcBlancTileBreakOffset = ri.Cvar_Get( "r_arcBlancTileBreakOffset", "-500", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_arcBlancTileBreakOffset, "World-space offset for secondary tile-break height sampling." );
	r_arcBlancTileBreakBlend = ri.Cvar_Get( "r_arcBlancTileBreakBlend", "0.45", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_arcBlancTileBreakBlend, "Blend strength for tile-break sampling." );
	r_arcBlancTileBreakCell = ri.Cvar_Get( "r_arcBlancTileBreakCell", "768", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_arcBlancTileBreakCell, "Cell size for tile-break noise variation." );
	r_arcBlancLakeMode = ri.Cvar_Get( "r_arcBlancLakeMode", "0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_arcBlancLakeMode, "Clamp Arc Blanc rendering to a finite rotated lake footprint." );
	r_arcBlancLakeCenter = ri.Cvar_Get( "r_arcBlancLakeCenter", "0 0 0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_arcBlancLakeCenter, "Lake center (x y z) when r_arcBlancLakeMode 1 or follow-camera is disabled." );
	r_arcBlancLakeExtents = ri.Cvar_Get( "r_arcBlancLakeExtents", "1024 1024", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_arcBlancLakeExtents, "Lake half extents in world units (x z)." );
	r_arcBlancLakeAngle = ri.Cvar_Get( "r_arcBlancLakeAngle", "0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_arcBlancLakeAngle, "Lake rotation angle in degrees." );
	ri.Printf( PRINT_ALL, "[VK][ArcBlanc] chocolate path ready (r_arcBlanc 0; USE_ARC_BLANC=ON)\n" );
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

static float R_ArcBlanc_SampleHeightArtDirected( float x, float z )
{
	float base = R_ArcBlanc_SampleHeight( x, z );

	if ( r_arcBlancTileBreak && r_arcBlancTileBreak->integer ) {
		float cellSize = r_arcBlancTileBreakCell ? r_arcBlancTileBreakCell->value : 768.0f;
		float offset = r_arcBlancTileBreakOffset ? r_arcBlancTileBreakOffset->value : -500.0f;
		float blend = r_arcBlancTileBreakBlend ? r_arcBlancTileBreakBlend->value : 0.45f;
		if ( cellSize > 8.0f && fabsf( offset ) > 1e-3f && blend > 0.0f ) {
			float noise = ab_value_noise2( x / cellSize, z / cellSize );
			float weight = ab_smoothstep( 0.2f, 0.8f, noise ) * ab_clampf01( blend );
			float alt = R_ArcBlanc_SampleHeight( x + offset, z - offset );
			base = base + ( alt - base ) * weight;
		}
	}

	return base;
}

static void R_ArcBlanc_ComputeNormal( float x, float z, float eps, vec3_t out )
{
	float normalStrength = r_arcBlancNormalStrength ? r_arcBlancNormalStrength->value : 1.0f;
	const float hL = R_ArcBlanc_SampleHeightArtDirected( x - eps, z );
	const float hR = R_ArcBlanc_SampleHeightArtDirected( x + eps, z );
	const float hD = R_ArcBlanc_SampleHeightArtDirected( x, z - eps );
	const float hU = R_ArcBlanc_SampleHeightArtDirected( x, z + eps );
	out[0] = ( hL - hR ) * normalStrength;
	out[1] = 2.0f * eps;
	out[2] = ( hD - hU ) * normalStrength;
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
			const float y = R_ArcBlanc_SampleHeightArtDirected( x, z );
			byte depthColor;
			float foamAmount = 0.0f;

			vert->xyz[0] = x;
			vert->xyz[1] = y;
			vert->xyz[2] = z;
			vert->st[0] = (float)w / (float)div;
			vert->st[1] = (float)h / (float)div;
			vert->lightmap[0] = 0.0f;
			vert->lightmap[1] = 0.0f;
			R_ArcBlanc_ComputeNormal( x, z, eps, vert->normal );
			if ( r_arcBlancFoam && r_arcBlancFoam->integer ) {
				float threshold = r_arcBlancFoamThreshold ? r_arcBlancFoamThreshold->value : 0.28f;
				float intensity = r_arcBlancFoamIntensity ? r_arcBlancFoamIntensity->value : 0.35f;
				float softness = r_arcBlancFoamSoftness ? r_arcBlancFoamSoftness->value : 1.5f;
				float steepness = 1.0f - vert->normal[1];
				foamAmount = ab_clampf01( ( steepness - threshold ) / softness ) * intensity;
			}
			depthColor = (byte)Com_Clamp( 0, 255, 140 + (int)( y * 4.0f ) );
			vert->color.rgba[0] = (byte)Com_Clamp( 0, 255, 20 + (int)( foamAmount * 235.0f ) );
			vert->color.rgba[1] = (byte)Com_Clamp( 0, 255, 90 + (int)( foamAmount * 165.0f ) );
			vert->color.rgba[2] = (byte)Com_Clamp( 0, 255, depthColor + (int)( foamAmount * 80.0f ) );
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
	float anchorX, anchorZ;
	float seaLevel;
	surfaceType_t *surf;
	int tiles = 1;
	int tx, tz;
	qboolean lakeMode = qfalse;
	vec3_t lakeCenter = { 0.0f, 0.0f, 0.0f };
	float lakeExtentX = 0.0f;
	float lakeExtentZ = 0.0f;
	float lakeAngleDeg = 0.0f;
	float lakeAngleRad = 0.0f;
	float lakeCos = 1.0f;
	float lakeSin = 0.0f;

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
	seaLevel = r_arcBlancSeaLevel ? r_arcBlancSeaLevel->value : 0.0f;
	tiles = r_arcBlancTileRadius ? r_arcBlancTileRadius->integer : 1;
	if ( tiles < 0 ) {
		tiles = 0;
	}

	lakeMode = ( r_arcBlancLakeMode && r_arcBlancLakeMode->integer != 0 ) ? qtrue : qfalse;
	if ( ab_parse_vec3( r_arcBlancLakeCenter ? r_arcBlancLakeCenter->string : NULL, lakeCenter ) ) {
		anchorX = lakeCenter[0];
		anchorZ = lakeCenter[2];
	} else {
		anchorX = tr.viewParms.or.origin[0];
		anchorZ = tr.viewParms.or.origin[2];
	}
	if ( r_arcBlancFollowCamera && r_arcBlancFollowCamera->integer ) {
		anchorX = tr.viewParms.or.origin[0];
		anchorZ = tr.viewParms.or.origin[2];
	}
	if ( lakeMode ) {
		ab_parse_vec2( r_arcBlancLakeExtents ? r_arcBlancLakeExtents->string : NULL, &lakeExtentX, &lakeExtentZ );
		if ( lakeExtentX < tileSize ) {
			lakeExtentX = tileSize;
		}
		if ( lakeExtentZ < tileSize ) {
			lakeExtentZ = tileSize;
		}
		lakeAngleDeg = r_arcBlancLakeAngle ? r_arcBlancLakeAngle->value : 0.0f;
		lakeAngleRad = lakeAngleDeg * (float)M_PI / 180.0f;
		lakeCos = cosf( lakeAngleRad );
		lakeSin = sinf( lakeAngleRad );
	}

	originX = floorf( anchorX / tileSize ) * tileSize;
	originZ = floorf( anchorZ / tileSize ) * tileSize;

	tr.currentEntityNum = REFENTITYNUM_WORLD;
	tr.shiftedEntityNum = tr.currentEntityNum << QSORT_REFENTITYNUM_SHIFT;

	for ( tz = -tiles; tz <= tiles; tz++ ) {
		for ( tx = -tiles; tx <= tiles; tx++ ) {
			const float ox = originX + (float)tx * tileSize;
			const float oz = originZ + (float)tz * tileSize;
			const float cx = ox + tileSize * 0.5f;
			const float cz = oz + tileSize * 0.5f;
			vec3_t bounds[2];

			if ( lakeMode ) {
				float dx = cx - lakeCenter[0];
				float dz = cz - lakeCenter[2];
				float lx = lakeCos * dx + lakeSin * dz;
				float lz = -lakeSin * dx + lakeCos * dz;
				if ( fabsf( lx ) > lakeExtentX || fabsf( lz ) > lakeExtentZ ) {
					continue;
				}
			}

			VectorSet( bounds[0], ox, -4096.0f, oz );
			VectorSet( bounds[1], ox + tileSize, 4096.0f, oz + tileSize );
			if ( R_CullLocalBox( bounds ) == CULL_OUT ) {
				continue;
			}

			{
				int patchDiv = div;
				if ( r_arcBlancAdaptiveMesh && r_arcBlancAdaptiveMesh->integer ) {
					int farDiv = r_arcBlancMeshDivFar ? r_arcBlancMeshDivFar->integer : 20;
					if ( farDiv > patchDiv ) {
						farDiv = patchDiv;
					}
					patchDiv = R_ArcBlanc_AdaptiveDiv( patchDiv, farDiv, tx, tz, tiles, seaLevel );
				}
				surf = R_ArcBlanc_BuildPatch( patchDiv, tileSize, ox, oz );
			}
			if ( surf ) {
				R_AddDrawSurf( surf, s_arcBlancShader, 0, 0 );
			}
		}
	}
}

#else /* !USE_ARC_BLANC */

void R_ArcBlanc_Init( void )
{
	static qboolean s_logged;

	(void)ri.Cvar_Get( "r_arcBlanc", "0", CVAR_ARCHIVE );
	(void)ri.Cvar_Get( "r_arcBlancDraw", "1", CVAR_ARCHIVE );
	if ( !s_logged ) {
		ri.Printf( PRINT_ALL, "[VK][ArcBlanc] stub (build with -DUSE_ARC_BLANC=ON)\n" );
		s_logged = qtrue;
	}
	VK_ArcBlancGpu_Init();
}

void R_ArcBlanc_AddSurfaces( void ) {}

void VK_ArcBlanc_Shutdown( void )
{
	VK_ArcBlancGpu_Shutdown();
}

void RE_ArcBlancUploadHeightMap( const byte *rgba, int width, int height )
{
	(void)rgba;
	(void)width;
	(void)height;
}

#endif /* USE_ARC_BLANC */
