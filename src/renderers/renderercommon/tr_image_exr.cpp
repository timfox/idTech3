/*
===========================================================================

OpenEXR image loader for idtech3

This adds support for loading .exr images via the OpenEXR library.
If OpenEXR is not available at build time, this file will compile a
stub loader that simply fails at runtime.

===========================================================================
*/

#include "../../common/q_shared.h"
#include "../renderercommon/tr_public.h"

extern refimport_t ri;

// We deliberately do not include tr_common.h here; the R_LoadEXR prototype
// is provided there for the renderer frontends, but not needed for the
// implementation.

// Renderer import interface - defined in renderer main file
extern "C" {
}

// Only use OpenEXR if the build system found it and defined USE_OPENEXR.
#ifdef USE_OPENEXR

// OpenEXR headers (version-agnostic includes)
#include <ImfRgbaFile.h>
#include <ImfArray.h>
#include <ImathBox.h>

#include <cmath>

// Helper to convert linear HDR float to 8-bit sRGB
static inline byte FloatToByteSRGB( float v )
{
	// Clamp to [0, 1]
	if ( v < 0.0f ) {
		v = 0.0f;
	} else if ( v > 1.0f ) {
		v = 1.0f;
	}

	// Simple gamma correction from linear to sRGB-ish space
	const float gamma = 1.0f / 2.2f;
	float srgb = std::pow( v, gamma );

	int iv = static_cast<int>( srgb * 255.0f + 0.5f );
	if ( iv < 0 ) {
		iv = 0;
	} else if ( iv > 255 ) {
		iv = 255;
	}

	return static_cast<byte>( iv );
}

/*
================
R_LoadEXR

Load an OpenEXR image and convert it into 8-bit RGBA.
If loading fails, *pic is set to NULL and width/height are zeroed.
================
*/
extern "C" void R_LoadEXR( const char *filename, byte **pic, int *width, int *height )
{
	using namespace Imf;
	using namespace Imath;

	if ( !pic ) {
		return;
	}

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

	try {
		RgbaInputFile file( filename );
		const Box2i &dw = file.dataWindow();

		const int w = dw.max.x - dw.min.x + 1;
		const int h = dw.max.y - dw.min.y + 1;

		if ( w <= 0 || h <= 0 ) {
			return;
		}

		// Allocate pixel storage in OpenEXR's preferred layout
		Array2D<Rgba> pixels;
		pixels.resizeErase( h, w );

		file.setFrameBuffer( &pixels[0][0] - dw.min.x - dw.min.y * w, 1, w );
		file.readPixels( dw.min.y, dw.max.y );

		// Allocate engine-side 8-bit RGBA buffer
		const size_t numPixels = static_cast<size_t>( w ) * static_cast<size_t>( h );
		byte *out = (byte *)ri.Malloc( numPixels * 4 );
		if ( !out ) {
			return;
		}

		for ( int y = 0; y < h; ++y ) {
			for ( int x = 0; x < w; ++x ) {
				const Rgba &p = pixels[y][x];

				const float r = (float)p.r;
				const float g = (float)p.g;
				const float b = (float)p.b;
				const float a = (float)p.a;

				const size_t idx = ( (size_t)y * (size_t)w + (size_t)x ) * 4;

				out[idx + 0] = FloatToByteSRGB( r );
				out[idx + 1] = FloatToByteSRGB( g );
				out[idx + 2] = FloatToByteSRGB( b );
				out[idx + 3] = FloatToByteSRGB( a );
			}
		}

		*pic = out;
		if ( width ) {
			*width = w;
		}
		if ( height ) {
			*height = h;
		}
	} catch ( const std::exception &e ) {
		ri.Printf( PRINT_DEVELOPER, "R_LoadEXR: failed to load '%s': %s\n", filename, e.what() );
		*pic = NULL;
		if ( width ) {
			*width = 0;
		}
		if ( height ) {
			*height = 0;
		}
	} catch ( ... ) {
		ri.Printf( PRINT_DEVELOPER, "R_LoadEXR: unknown error while loading '%s'\n", filename );
		*pic = NULL;
		if ( width ) {
			*width = 0;
		}
		if ( height ) {
			*height = 0;
		}
	}
}

#else // !USE_OPENEXR

// Stub implementation when OpenEXR support is not compiled in.
extern "C" void R_LoadEXR( const char *filename, byte **pic, int *width, int *height )
{
	if ( pic ) {
		*pic = NULL;
	}
	if ( width ) {
		*width = 0;
	}
	if ( height ) {
		*height = 0;
	}

	ri.Printf( PRINT_DEVELOPER, "R_LoadEXR: OpenEXR support not available (file '%s')\n",
		filename ? filename : "(null)" );
}

#endif // USE_OPENEXR


