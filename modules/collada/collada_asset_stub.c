/*
 * Fallback Collada API for builds without the optional FCollada dependency.
 * Runtime path classification remains available to the game trap; conversion
 * entry points report that the optional converter is unavailable.
 */

#include "collada_public.h"

#include <string.h>

static int Collada_EndsWithNoCase( const char *path, const char *suffix ) {
	size_t pathLen = path ? strlen( path ) : 0;
	size_t suffixLen = suffix ? strlen( suffix ) : 0;
	if ( pathLen < suffixLen ) return 0;
	path += pathLen - suffixLen;
	while ( suffixLen-- ) {
		char a = *path++;
		char b = *suffix++;
		if ( a >= 'A' && a <= 'Z' ) a = (char)( a - 'A' + 'a' );
		if ( b >= 'A' && b <= 'Z' ) b = (char)( b - 'A' + 'a' );
		if ( a != b ) return 0;
	}
	return 1;
}

int Collada_IsSourcePath( const char *path ) {
	return Collada_EndsWithNoCase( path, ".dae" );
}

colladaAssetType_t Collada_ClassifyRuntimePath( const char *path ) {
	if ( Collada_EndsWithNoCase( path, ".pmd" ) ) return COLLADA_ASSET_MESH;
	if ( Collada_EndsWithNoCase( path, ".psa" ) ) return COLLADA_ASSET_ANIMATION;
	return COLLADA_ASSET_UNKNOWN;
}

int Collada_GetRuntimePath( const char *sourcePath, colladaAssetType_t type, char *out, int outSize ) {
	const char *extension;
	size_t baseLen;
	size_t extensionLen;
	const char *dot;

	if ( !Collada_IsSourcePath( sourcePath ) || !out || outSize <= 0 ) return 0;
	extension = type == COLLADA_ASSET_MESH ? ".pmd" : type == COLLADA_ASSET_ANIMATION ? ".psa" : NULL;
	if ( !extension ) { out[0] = '\0'; return 0; }
	dot = strrchr( sourcePath, '.' );
	baseLen = dot ? (size_t)( dot - sourcePath ) : strlen( sourcePath );
	extensionLen = strlen( extension );
	if ( baseLen + extensionLen + 1 > (size_t)outSize ) { out[0] = '\0'; return 0; }
	memcpy( out, sourcePath, baseLen );
	memcpy( out + baseLen, extension, extensionLen + 1 );
	return 1;
}

int Collada_SetSkeletonDefinitions( const char *xml, int length ) {
	(void)xml; (void)length; return 0;
}

int Collada_ConvertDaeToPmd( const char *dae, colladaOutputFn_t writer, void *userData ) {
	(void)dae; (void)writer; (void)userData; return 0;
}

int Collada_ConvertDaeToPsa( const char *dae, colladaOutputFn_t writer, void *userData ) {
	(void)dae; (void)writer; (void)userData; return 0;
}
