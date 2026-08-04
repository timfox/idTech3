/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Register .usd / .usda models via FreeUSD (C) + vertex-soup fallback.
===========================================================================
*/

#include <string.h>

#include "tr_model_freeusd.h"
#include "tr_model_mesh_import.h"

#if defined( RENDERER_VULKAN )
#include "../vulkan/tr_local.h"
#else
#error tr_model_freeusd_register.c requires RENDERER_VULKAN
#endif

#ifdef USE_FREEUSD
#include "freeusd/c/freeusd.h"
#endif

static cvar_t *r_freeusd = NULL;

#ifndef USE_FREEUSD
qboolean R_Freeusd_BuildMeshBuffers( const char *qpath, float **verts, int *numVerts,
	int **inds, int *numIdx, float **vertSt, char *shaderNameOut, int shaderNameOutSize ) {
	(void)qpath;
	(void)verts;
	(void)numVerts;
	(void)inds;
	(void)numIdx;
	(void)vertSt;
	(void)shaderNameOut;
	(void)shaderNameOutSize;
	return qfalse;
}
#endif

qboolean R_Freeusd_MeshImportEnabled( void ) {
#ifdef USE_FREEUSD
	return r_freeusd && r_freeusd->integer;
#else
	return qfalse;
#endif
}

void R_Freeusd_Init( void ) {
	cvar_t *pick;
	cvar_t *idx;
	cvar_t *time;
	cvar_t *pathFilter;

	r_freeusd = ri.Cvar_Get( "r_freeusd", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_freeusd,
		"When 1 and built with USE_FREEUSD, .usd/.usda models load via FreeUSD UsdGeom.Mesh (else vertex-soup fallback)." );

	pick = ri.Cvar_Get( "r_freeusdPickLargest", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( pick,
		"When 1, import the UsdGeom.Mesh with the most triangles; when 0, use r_freeusdMeshIndex." );

	idx = ri.Cvar_Get( "r_freeusdMeshIndex", "0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( idx,
		"Mesh index (0-based) when r_freeusdPickLargest is 0 (see usd_meshes for ordering)." );

	time = ri.Cvar_Get( "r_freeusdTime", "1.0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( time, "USD time code for mesh point/topology samples (default 1.0)." );

	pathFilter = ri.Cvar_Get( "r_freeusdMeshPath", "", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( pathFilter,
		"Optional substring filter on prim path when selecting a mesh (empty = all meshes)." );

	{
		cvar_t *shaderMap = ri.Cvar_Get( "r_freeusdShaderMap", "1", CVAR_ARCHIVE );
		ri.Cvar_SetDescription( shaderMap,
			"When 1, map UsdPreviewSurface material:binding to Q3 shader paths (diffuse texture asset)." );
	}

#ifdef USE_FREEUSD
	{
		const char *ver = freeusd_version_string();
		if ( ver && ver[0] ) {
			ri.Printf( PRINT_ALL, "FreeUSD mesh import enabled (%s, r_freeusd=%d)\n",
				ver, r_freeusd ? r_freeusd->integer : 0 );
		}
	}
#endif
}

qhandle_t R_RegisterFreeusdMesh( const char *name, model_t *mod ) {
#ifdef USE_FREEUSD
	if ( !R_Freeusd_MeshImportEnabled() ) {
		return 0;
	}

	char filename[MAX_QPATH];
	char *dot;
	const char *fext;
	int lod;
	int numLoaded = 0;

	Q_strncpyz( filename, name, sizeof( filename ) );
	dot = strrchr( filename, '.' );
	if ( !dot ) {
		mod->type = MOD_BAD;
		return 0;
	}
	fext = dot + 1;

	for ( lod = MD3_MAX_LODS - 1; lod >= 0; lod-- ) {
		char namebuf[MAX_QPATH + 32];
		float *verts = NULL;
		int numVerts = 0;
		int *inds = NULL;
		int numIdx = 0;
		float *vertSt = NULL;
		char shaderName[R_FREEUSD_SHADERNAME_MAX];
		qboolean ok;
		if ( !Q_stricmp( fext, "usd" ) || !Q_stricmp( fext, "usda" ) ) {
			/* USDA LODs are authored as separate assets, not by the MD3
			 * suffix convention.  Probe the requested asset once. */
			if ( lod != 0 ) {
				continue;
			}
		}

		if ( lod ) {
			Com_sprintf( namebuf, sizeof( namebuf ), "%.*s_%d.%s",
				(int)( dot - filename ), filename, lod, fext );
		} else {
			Q_strncpyz( namebuf, name, sizeof( namebuf ) );
		}

		/* Do not use the legacy FS_ReadFile existence probe for USDA.  It
		 * materializes the whole layer in the hunk before FreeUSD can stream or
		 * parse it, which is fatal for large validation scenes such as Sponza. */
		if ( Q_stricmp( fext, "usd" ) && Q_stricmp( fext, "usda" ) &&
			ri.FS_ReadFile( namebuf, NULL ) <= 0 ) {
			continue;
		}

		shaderName[0] = '\0';
		if ( !R_Freeusd_BuildMeshBuffers( namebuf, &verts, &numVerts, &inds, &numIdx, &vertSt,
				shaderName, (int)sizeof( shaderName ) ) ) {
			if ( verts ) {
				ri.Free( verts );
			}
			if ( inds ) {
				ri.Free( inds );
			}
			if ( vertSt ) {
				ri.Free( vertSt );
			}
			break;
		}

		ok = R_MeshImport_FinalizeMD3Ex( mod, lod, namebuf, verts, numVerts, inds, numIdx,
			shaderName[0] ? shaderName : NULL, vertSt, NULL );
		ri.Free( verts );
		ri.Free( inds );
		if ( vertSt ) {
			ri.Free( vertSt );
		}
		if ( !ok ) {
			break;
		}
		mod->numLods++;
		numLoaded++;
	}

	if ( !numLoaded ) {
		mod->numLods = 0;
		Com_Memset( mod->md3, 0, sizeof( mod->md3 ) );
		return 0;
	}

	for ( lod--; lod >= 0; lod-- ) {
		mod->numLods++;
		mod->md3[lod] = mod->md3[lod + 1];
	}
	return mod->index;
#endif

	(void)name;
	(void)mod;
	return 0;
}
