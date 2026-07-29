/*
===========================================================================
Engine-owned Collada asset support.

Game modules should not embed Collada import/conversion code. They should ask
the engine asset layer for loadable runtime assets and keep game rules separate.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum colladaAssetType_e {
	COLLADA_ASSET_UNKNOWN = 0,
	COLLADA_ASSET_MESH,
	COLLADA_ASSET_ANIMATION
} colladaAssetType_t;

typedef void (*colladaOutputFn_t)( void *userData, const char *data, unsigned length );

int Collada_IsSourcePath( const char *path );
colladaAssetType_t Collada_ClassifyRuntimePath( const char *path );
int Collada_GetRuntimePath( const char *sourcePath, colladaAssetType_t type,
	char *out, int outSize );
int Collada_SetSkeletonDefinitions( const char *xml, int length );
int Collada_ConvertDaeToPmd( const char *dae, colladaOutputFn_t writer, void *userData );
int Collada_ConvertDaeToPsa( const char *dae, colladaOutputFn_t writer, void *userData );

#ifdef __cplusplus
}
#endif
