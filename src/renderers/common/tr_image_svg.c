/*
===========================================================================
SVG image loader (optional librsvg backend)

Provides rasterization for .svg assets when USE_LIBRSVG is enabled.
Falls back cleanly when the backend is unavailable.
===========================================================================
*/

#include "../../qcommon/q_shared.h"
#include "../../qcommon/qcommon.h"
#include "tr_image_loaders.h"

#ifdef USE_LIBRSVG
#include <librsvg/rsvg.h>
#include <cairo/cairo.h>

static int SVG_ClampInt( int value, int minValue, int maxValue ) {
	if ( value < minValue ) {
		return minValue;
	}
	if ( value > maxValue ) {
		return maxValue;
	}
	return value;
}

static float SVG_ClampFloat( float value, float minValue, float maxValue ) {
	if ( value < minValue ) {
		return minValue;
	}
	if ( value > maxValue ) {
		return maxValue;
	}
	return value;
}

static byte SVG_Unpremultiply( byte c, byte a ) {
	int value;

	if ( a == 0 ) {
		return 0;
	}
	value = ( (int)c * 255 + ( a / 2 ) ) / a;
	if ( value < 0 ) {
		return 0;
	}
	if ( value > 255 ) {
		return 255;
	}
	return (byte)value;
}

void R_LoadSVG( const char *filename, byte **pic, int *width, int *height ) {
	void *fileData = NULL;
	int fileSize;
	int maxFileBytes;
	int maxRasterSize;
	float rasterScale;
	RsvgHandle *handle = NULL;
	GError *error = NULL;
	RsvgDimensionData dim;
	cairo_surface_t *surface = NULL;
	cairo_t *cr = NULL;
	byte *out = NULL;
	int targetW;
	int targetH;
	int x, y;

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

	maxFileBytes = SVG_ClampInt( Cvar_VariableIntegerValue( "r_svgMaxFileBytes" ), 64 * 1024, 32 * 1024 * 1024 );
	if ( maxFileBytes <= 0 ) {
		maxFileBytes = 2 * 1024 * 1024;
	}
	maxRasterSize = SVG_ClampInt( Cvar_VariableIntegerValue( "r_svgMaxRasterSize" ), 64, 8192 );
	if ( maxRasterSize <= 0 ) {
		maxRasterSize = 4096;
	}
	rasterScale = SVG_ClampFloat( Cvar_VariableValue( "r_svgRasterScale" ), 0.1f, 8.0f );
	if ( rasterScale <= 0.0f ) {
		rasterScale = 1.0f;
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

	handle = rsvg_handle_new_from_data( (const guint8 *)fileData, (gsize)fileSize, &error );
	FS_FreeFile( fileData );
	fileData = NULL;

	if ( !handle ) {
		Com_Printf( S_COLOR_YELLOW "SVG: failed to parse '%s': %s\n",
			filename, error ? error->message : "unknown error" );
		if ( error ) {
			g_error_free( error );
		}
		return;
	}

	/* Disable external resource resolution by using an empty base URI. */
	rsvg_handle_set_base_uri( handle, "" );

	rsvg_handle_get_dimensions( handle, &dim );
	if ( dim.width <= 0 || dim.height <= 0 ) {
		Com_Printf( S_COLOR_YELLOW "SVG: invalid dimensions in '%s' (%d x %d)\n",
			filename, dim.width, dim.height );
		g_object_unref( handle );
		return;
	}

	targetW = (int)( (float)dim.width * rasterScale );
	targetH = (int)( (float)dim.height * rasterScale );
	if ( targetW <= 0 || targetH <= 0 ) {
		g_object_unref( handle );
		return;
	}
	if ( targetW > maxRasterSize || targetH > maxRasterSize ) {
		Com_Printf( S_COLOR_YELLOW "SVG: '%s' raster target too large (%d x %d, max %d)\n",
			filename, targetW, targetH, maxRasterSize );
		g_object_unref( handle );
		return;
	}

	surface = cairo_image_surface_create( CAIRO_FORMAT_ARGB32, targetW, targetH );
	if ( !surface || cairo_surface_status( surface ) != CAIRO_STATUS_SUCCESS ) {
		if ( surface ) {
			cairo_surface_destroy( surface );
		}
		g_object_unref( handle );
		return;
	}

	cr = cairo_create( surface );
	if ( !cr || cairo_status( cr ) != CAIRO_STATUS_SUCCESS ) {
		if ( cr ) {
			cairo_destroy( cr );
		}
		cairo_surface_destroy( surface );
		g_object_unref( handle );
		return;
	}

	cairo_set_operator( cr, CAIRO_OPERATOR_CLEAR );
	cairo_paint( cr );
	cairo_set_operator( cr, CAIRO_OPERATOR_OVER );

	if ( targetW != dim.width || targetH != dim.height ) {
		cairo_scale( cr, (double)targetW / (double)dim.width, (double)targetH / (double)dim.height );
	}

	if ( !rsvg_handle_render_cairo( handle, cr ) ) {
		Com_Printf( S_COLOR_YELLOW "SVG: render failed for '%s'\n", filename );
		cairo_destroy( cr );
		cairo_surface_destroy( surface );
		g_object_unref( handle );
		return;
	}

	cairo_surface_flush( surface );

	out = (byte *)ri.Malloc( targetW * targetH * 4 );
	for ( y = 0; y < targetH; y++ ) {
		const byte *src = cairo_image_surface_get_data( surface ) + ( y * cairo_image_surface_get_stride( surface ) );
		byte *dst = out + ( y * targetW * 4 );
		for ( x = 0; x < targetW; x++, src += 4, dst += 4 ) {
			const byte b = src[0];
			const byte g = src[1];
			const byte r = src[2];
			const byte a = src[3];

			dst[0] = SVG_Unpremultiply( r, a );
			dst[1] = SVG_Unpremultiply( g, a );
			dst[2] = SVG_Unpremultiply( b, a );
			dst[3] = a;
		}
	}

	cairo_destroy( cr );
	cairo_surface_destroy( surface );
	g_object_unref( handle );

	*pic = out;
	if ( width ) {
		*width = targetW;
	}
	if ( height ) {
		*height = targetH;
	}
}

#else

void R_LoadSVG( const char *filename, byte **pic, int *width, int *height ) {
	(void)filename;
	*pic = NULL;
	if ( width ) {
		*width = 0;
	}
	if ( height ) {
		*height = 0;
	}
}

#endif /* USE_LIBRSVG */
