/*
===========================================================================
Engine-owned Collada asset support.
===========================================================================
*/

#include "collada_public.h"
#include "converter/DLL.h"

#include <cctype>
#include <cstring>

namespace {

static int EndsWithNoCase( const char *text, const char *suffix ) {
	const size_t textLen = text ? std::strlen( text ) : 0;
	const size_t suffixLen = suffix ? std::strlen( suffix ) : 0;

	if ( textLen < suffixLen ) {
		return 0;
	}
	for ( size_t i = 0; i < suffixLen; ++i ) {
		const unsigned char a = static_cast<unsigned char>( text[textLen - suffixLen + i] );
		const unsigned char b = static_cast<unsigned char>( suffix[i] );
		if ( std::tolower( a ) != std::tolower( b ) ) {
			return 0;
		}
	}
	return 1;
}

static int ReplaceExtension( const char *sourcePath, const char *extension, char *out, int outSize ) {
	const char *dot;
	size_t baseLen;
	size_t extensionLen;

	if ( !sourcePath || !extension || !out || outSize <= 0 ) {
		return 0;
	}

	dot = std::strrchr( sourcePath, '.' );
	baseLen = dot ? static_cast<size_t>( dot - sourcePath ) : std::strlen( sourcePath );
	extensionLen = std::strlen( extension );
	if ( baseLen + extensionLen + 1 > static_cast<size_t>( outSize ) ) {
		out[0] = '\0';
		return 0;
	}

	std::memcpy( out, sourcePath, baseLen );
	std::memcpy( out + baseLen, extension, extensionLen + 1 );
	return 1;
}

}  // namespace

extern "C" {

int Collada_IsSourcePath( const char *path ) {
	return EndsWithNoCase( path, ".dae" );
}

colladaAssetType_t Collada_ClassifyRuntimePath( const char *path ) {
	if ( EndsWithNoCase( path, ".pmd" ) ) {
		return COLLADA_ASSET_MESH;
	}
	if ( EndsWithNoCase( path, ".psa" ) ) {
		return COLLADA_ASSET_ANIMATION;
	}
	return COLLADA_ASSET_UNKNOWN;
}

int Collada_GetRuntimePath( const char *sourcePath, colladaAssetType_t type, char *out, int outSize ) {
	if ( !Collada_IsSourcePath( sourcePath ) ) {
		if ( out && outSize > 0 ) {
			out[0] = '\0';
		}
		return 0;
	}

	switch ( type ) {
	case COLLADA_ASSET_MESH:
		return ReplaceExtension( sourcePath, ".pmd", out, outSize );
	case COLLADA_ASSET_ANIMATION:
		return ReplaceExtension( sourcePath, ".psa", out, outSize );
	case COLLADA_ASSET_UNKNOWN:
	default:
		if ( out && outSize > 0 ) {
			out[0] = '\0';
		}
		return 0;
	}
}

int Collada_ConvertDaeToPmd( const char *dae, colladaOutputFn_t writer, void *userData ) {
	return convert_dae_to_pmd( dae, reinterpret_cast<OutputFn>( writer ), userData );
}

int Collada_SetSkeletonDefinitions( const char *xml, int length ) {
	return set_skeleton_definitions( xml, length );
}

int Collada_ConvertDaeToPsa( const char *dae, colladaOutputFn_t writer, void *userData ) {
	return convert_dae_to_psa( dae, reinterpret_cast<OutputFn>( writer ), userData );
}

}
