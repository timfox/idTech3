/*
===========================================================================
SVG image loader (NanoSVG + NanoSVG rasterizer, vendored headers).

Rasterizes .svg assets on all platforms without external dependencies.
===========================================================================
*/

#include "../../qcommon/q_shared.h"
#include "../../qcommon/qcommon.h"
#include "tr_image_loaders.h"
#include "tr_public.h"

#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#include "../../external/include/nanosvg/nanosvg.h"
#include "../../external/include/nanosvg/nanosvgrast.h"

void R_LoadSVG( const char *filename, byte **pic, int *width, int *height ) {
	void *fileData = NULL;
	int fileSize;
	int maxFileBytes, maxRasterSize;
	float rasterScale;
	NSVGimage *image = NULL;
	NSVGrasterizer *rast = NULL;
	byte *out = NULL;
	int targetW, targetH;

	*pic = NULL;
	if ( width ) {
		*width = 0;
	}
	if ( height ) {
		*height = 0;
	}
	if ( !filename || !filename[0] ) {
		return;
	}

	maxFileBytes = Cvar_VariableIntegerValue( "r_svgMaxFileBytes" );
	if ( maxFileBytes < 64 * 1024 ) {
		maxFileBytes = 64 * 1024;
	}
	if ( maxFileBytes > 32 * 1024 * 1024 ) {
		maxFileBytes = 32 * 1024 * 1024;
	}
	maxRasterSize = Cvar_VariableIntegerValue( "r_svgMaxRasterSize" );
	if ( maxRasterSize < 64 ) {
		maxRasterSize = 64;
	}
	if ( maxRasterSize > 8192 ) {
		maxRasterSize = 8192;
	}
	rasterScale = Cvar_VariableValue( "r_svgRasterScale" );
	if ( rasterScale < 0.1f ) {
		rasterScale = 0.1f;
	}
	if ( rasterScale > 8.0f ) {
		rasterScale = 8.0f;
	}

	fileSize = FS_ReadFile( filename, &fileData );
	if ( fileSize <= 0 || !fileData ) {
		return;
	}
	if ( fileSize > maxFileBytes ) {
		Com_Printf( S_COLOR_YELLOW "SVG: '%s' exceeds max file size (%d > %d)\n",
			filename, fileSize, maxFileBytes );
		FS_FreeFile( fileData );
		return;
	}

	{
		char *parseBuf = (char *)ri.Malloc( (size_t)fileSize + 1 );
		if ( !parseBuf ) {
			FS_FreeFile( fileData );
			return;
		}
		Com_Memcpy( parseBuf, fileData, (size_t)fileSize );
		parseBuf[fileSize] = '\0';
		FS_FreeFile( fileData );
		fileData = NULL;

		image = nsvgParse( parseBuf, "px", 96.0f );
		ri.Free( parseBuf );
	}
	if ( !image || image->width <= 0 || image->height <= 0 ) {
		if ( image ) {
			nsvgDelete( image );
		}
		return;
	}

	targetW = (int)( image->width * rasterScale );
	targetH = (int)( image->height * rasterScale );
	if ( targetW <= 0 || targetH <= 0 ) {
		nsvgDelete( image );
		return;
	}
	if ( targetW > maxRasterSize || targetH > maxRasterSize ) {
		Com_Printf( S_COLOR_YELLOW "SVG: '%s' raster target too large (%d x %d, max %d)\n",
			filename, targetW, targetH, maxRasterSize );
		nsvgDelete( image );
		return;
	}

	rast = nsvgCreateRasterizer();
	if ( !rast ) {
		nsvgDelete( image );
		return;
	}

	out = (byte *)ri.Malloc( (size_t)( targetW * targetH * 4 ) );
	if ( !out ) {
		nsvgDeleteRasterizer( rast );
		nsvgDelete( image );
		return;
	}

	nsvgRasterize( rast, image, 0, 0, rasterScale, out, targetW, targetH, targetW * 4 );

	nsvgDeleteRasterizer( rast );
	nsvgDelete( image );

	*pic = out;
	if ( width ) {
		*width = targetW;
	}
	if ( height ) {
		*height = targetH;
	}
}
