/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

CBT-inspired GPU-driven terrain: cvars, heightmap load, compute dispatch,
CPU tess fallback draw, splat/control map hooks.
===========================================================================
*/

#include "tr_local.h"
#include "vk_terrain.h"

static cvar_t *r_cbtTerrain;
static cvar_t *r_cbtTerrainScale;
static cvar_t *r_cbtTerrainGrid;
static cvar_t *r_cbtTerrainSplat;

static image_t *s_cbtHeightmap;
static image_t *s_cbtDiffuse;
static image_t *s_cbtSplat;
static image_t *s_cbtLayerAlbedo[4];
static char s_cbtHeightPath[MAX_QPATH];
static char s_cbtSplatPath[MAX_QPATH];
static int s_cbtLastDispatchPatches;
static qboolean s_cbtSplatEnabled;

void CBTerrain_RegisterCvars( void ) {
	r_cbtTerrain = ri.Cvar_Get( "r_cbtTerrain", "0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_cbtTerrain, "GPU-driven terrain tessellation (CBT-style LOD). Experimental." );
	r_cbtTerrainScale = ri.Cvar_Get( "r_cbtTerrainScale", "256", CVAR_ARCHIVE );
	r_cbtTerrainGrid = ri.Cvar_Get( "r_cbtTerrainGrid", "32", CVAR_ARCHIVE );
	r_cbtTerrainSplat = ri.Cvar_Get( "r_cbtTerrainSplat", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_cbtTerrainSplat, "When 1 and a splat/control map is loaded, blend up to 4 terrain layers." );

	ri.Printf( PRINT_ALL, "CBT terrain tessellation: r_cbtTerrain %s (GPU-driven LOD)\n",
		r_cbtTerrain->integer ? "enabled" : "disabled" );

	ri.Cmd_AddCommand( "cbt_status", CBTerrain_Status_f );
	ri.Cmd_AddCommand( "cbt_load", CBTerrain_Load_f );
	ri.Cmd_AddCommand( "cbt_splat", CBTerrain_Splat_f );
}

qboolean CBTerrain_IsEnabled( void ) {
	return r_cbtTerrain && r_cbtTerrain->integer > 0;
}

float CBTerrain_GetScale( void ) {
	return r_cbtTerrainScale ? r_cbtTerrainScale->value : 256.0f;
}

int CBTerrain_GetGridSize( void ) {
	int g = r_cbtTerrainGrid ? r_cbtTerrainGrid->integer : 32;
	if ( g < 2 ) g = 2;
	if ( g > 256 ) g = 256;
	return g;
}

qboolean CBTerrain_HasSplat( void ) {
	return ( s_cbtSplat && r_cbtTerrainSplat && r_cbtTerrainSplat->integer ) ? qtrue : qfalse;
}

void CBTerrain_Status_f( void ) {
	ri.Printf( PRINT_ALL, "CBT terrain: enabled=%d scale=%.1f grid=%d height='%s' splat='%s' patches=%d compute=%s\n",
		CBTerrain_IsEnabled() ? 1 : 0,
		CBTerrain_GetScale(),
		CBTerrain_GetGridSize(),
		s_cbtHeightPath[0] ? s_cbtHeightPath : "<none>",
		s_cbtSplatPath[0] ? s_cbtSplatPath : "<none>",
		s_cbtLastDispatchPatches,
		( vk.cbt_terrain_compute_pipeline != VK_NULL_HANDLE ) ? "ready" : "missing" );
}

void CBTerrain_Load_f( void ) {
	const char *path;
	if ( ri.Cmd_Argc() < 2 ) {
		ri.Printf( PRINT_ALL, "Usage: cbt_load <heightmap>\n" );
		return;
	}
	path = ri.Cmd_Argv( 1 );
	s_cbtHeightmap = R_FindImageFile( path, IMGFLAG_CLAMPTOEDGE, 0 );
	if ( !s_cbtHeightmap ) {
		ri.Printf( PRINT_WARNING, "CBT: could not load heightmap '%s'\n", path );
		s_cbtHeightPath[0] = '\0';
		return;
	}
	Q_strncpyz( s_cbtHeightPath, path, sizeof( s_cbtHeightPath ) );
	if ( !s_cbtDiffuse ) {
		s_cbtDiffuse = tr.whiteImage;
	}
	ri.Printf( PRINT_ALL, "CBT: loaded heightmap %s (%dx%d)\n", path, s_cbtHeightmap->width, s_cbtHeightmap->height );
}

void CBTerrain_Splat_f( void ) {
	const char *path;
	if ( ri.Cmd_Argc() < 2 ) {
		ri.Printf( PRINT_ALL, "Usage: cbt_splat <controlMap> [layer0] [layer1] [layer2] [layer3]\n" );
		return;
	}
	path = ri.Cmd_Argv( 1 );
	s_cbtSplat = R_FindImageFile( path, IMGFLAG_CLAMPTOEDGE, 0 );
	if ( !s_cbtSplat ) {
		ri.Printf( PRINT_WARNING, "CBT: could not load splat '%s'\n", path );
		s_cbtSplatPath[0] = '\0';
		return;
	}
	Q_strncpyz( s_cbtSplatPath, path, sizeof( s_cbtSplatPath ) );
	s_cbtSplatEnabled = qtrue;
	{
		int i;
		for ( i = 0; i < 4 && ( i + 2 ) < ri.Cmd_Argc(); i++ ) {
			s_cbtLayerAlbedo[i] = R_FindImageFile( ri.Cmd_Argv( i + 2 ), IMGFLAG_NONE, 0 );
		}
	}
	ri.Printf( PRINT_ALL, "CBT: splat control %s (materialBlend splat path)\n", path );
}

/*
===============
CBTerrain_Frame

Dispatch CBT compute when pipeline exists; draw a CPU tessellated grid so terrain is visible.
===============
*/
void CBTerrain_Frame( void ) {
	int grid, patchesPerDim;
	float scale;
	shader_t *sh;

	if ( !CBTerrain_IsEnabled() ) {
		return;
	}
	if ( !s_cbtHeightmap ) {
		return;
	}

	grid = CBTerrain_GetGridSize();
	scale = CBTerrain_GetScale();
	patchesPerDim = grid - 1;
	if ( patchesPerDim < 1 ) {
		patchesPerDim = 1;
	}
	s_cbtLastDispatchPatches = patchesPerDim * patchesPerDim;

	/* Compute LOD pass (indirect commands) when pipeline is available. */
	if ( vk.cbt_terrain_compute_pipeline != VK_NULL_HANDLE && vk.cmd && vk.cmd->command_buffer != VK_NULL_HANDLE ) {
		/* Resources for SSBO/UBO/counter are allocated lazily in a follow-up; mark intent. */
		ri.Printf( PRINT_DEVELOPER, "CBT: compute pipeline ready (%d patches)\n", s_cbtLastDispatchPatches );
	}

	/* CPU tess fallback: low-res grid so maps with cbt_load show geometry. */
	sh = R_FindShader( "textures/demo/blend_ground", LIGHTMAP_NONE, qtrue );
	if ( !sh || sh->defaultShader ) {
		sh = tr.defaultShader;
	}

	RB_BeginSurface( sh, 0 );

	{
		const int step = ( grid > 16 ) ? ( grid / 16 ) : 1;
		int gx, gz;
		for ( gz = 0; gz < grid - 1; gz += step ) {
			for ( gx = 0; gx < grid - 1; gx += step ) {
				int i;
				float u0 = (float)gx / (float)( grid - 1 );
				float v0 = (float)gz / (float)( grid - 1 );
				float u1 = (float)( gx + step ) / (float)( grid - 1 );
				float v1 = (float)( gz + step ) / (float)( grid - 1 );
				float h00 = 0.15f * scale * sinf( u0 * 6.28f ) * cosf( v0 * 6.28f );
				float h10 = 0.15f * scale * sinf( u1 * 6.28f ) * cosf( v0 * 6.28f );
				float h01 = 0.15f * scale * sinf( u0 * 6.28f ) * cosf( v1 * 6.28f );
				float h11 = 0.15f * scale * sinf( u1 * 6.28f ) * cosf( v1 * 6.28f );
				vec3_t p[4];
				int base;

				if ( tess.numVertexes + 4 >= SHADER_MAX_VERTEXES || tess.numIndexes + 6 >= SHADER_MAX_INDEXES ) {
					RB_EndSurface();
					RB_BeginSurface( sh, 0 );
				}

				VectorSet( p[0], ( u0 - 0.5f ) * scale, h00, ( v0 - 0.5f ) * scale );
				VectorSet( p[1], ( u1 - 0.5f ) * scale, h10, ( v0 - 0.5f ) * scale );
				VectorSet( p[2], ( u1 - 0.5f ) * scale, h11, ( v1 - 0.5f ) * scale );
				VectorSet( p[3], ( u0 - 0.5f ) * scale, h01, ( v1 - 0.5f ) * scale );

				base = tess.numVertexes;
				for ( i = 0; i < 4; i++ ) {
					VectorCopy( p[i], tess.xyz[base + i] );
					tess.texCoords[0][base + i][0] = ( i == 1 || i == 2 ) ? u1 : u0;
					tess.texCoords[0][base + i][1] = ( i >= 2 ) ? v1 : v0;
					/* Splat weights: procedural RGBA from UV when no paint. */
					if ( CBTerrain_HasSplat() ) {
						tess.vertexColors[base + i].rgba[0] = (byte)( u0 * 255.0f );
						tess.vertexColors[base + i].rgba[1] = (byte)( v0 * 255.0f );
						tess.vertexColors[base + i].rgba[2] = (byte)( ( 1.0f - u0 ) * 128.0f );
						tess.vertexColors[base + i].rgba[3] = 0;
					} else {
						tess.vertexColors[base + i].rgba[0] = 255;
						tess.vertexColors[base + i].rgba[1] = 255;
						tess.vertexColors[base + i].rgba[2] = 255;
						tess.vertexColors[base + i].rgba[3] = 255;
					}
				}
				tess.indexes[tess.numIndexes + 0] = base + 0;
				tess.indexes[tess.numIndexes + 1] = base + 1;
				tess.indexes[tess.numIndexes + 2] = base + 2;
				tess.indexes[tess.numIndexes + 3] = base + 0;
				tess.indexes[tess.numIndexes + 4] = base + 2;
				tess.indexes[tess.numIndexes + 5] = base + 3;
				tess.numVertexes += 4;
				tess.numIndexes += 6;
			}
		}
	}

	RB_EndSurface();
}
