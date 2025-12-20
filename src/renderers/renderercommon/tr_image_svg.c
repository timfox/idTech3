/*
extern refimport_t ri;
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "../common/q_shared.h"
#include "../renderercommon/tr_public.h"
#include "../renderer/tr_common.h"

// Renderer import interface - defined in renderer main file
extern refimport_t ri;





#ifdef USE_NANOSVG
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#pragma GCC diagnostic ignored "-Wuninitialized"
#endif
#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#include "../../libs/nanosvg/nanosvg.h"
#include "../../libs/nanosvg/nanosvgrast.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

void R_LoadSVG( const char *name, byte **pic, int *width, int *height )
{
	union {
		byte *b;
		void *v;
	} buffer;
	int length;
	NSVGimage *image = NULL;
	NSVGrasterizer *rast = NULL;
	unsigned char *img = NULL;
	int w, h;

	*pic = NULL;

	if ( width )
		*width = 0;

	if ( height )
		*height = 0;

	if ( !name || !name[0] )
		return;

	// Load the SVG file
	length = ri.FS_ReadFile( ( char * ) name, &buffer.v );
	if ( !buffer.b || length < 0 )
		return;

	// SVG is text-based, so we need null termination for the parser
	// Always create a null-terminated copy to be safe
	char *nullTerminated = (char *)ri.Malloc( length + 1 );
	if ( !nullTerminated )
	{
		ri.FS_FreeFile( buffer.v );
		return;
	}
	Com_Memcpy( nullTerminated, buffer.b, length );
	nullTerminated[length] = '\0';

	// Parse SVG
	image = nsvgParse( nullTerminated, "px", 96.0f );
	
	// Free the file buffer and temporary string
	ri.FS_FreeFile( buffer.v );
	ri.Free( nullTerminated );

	if ( !image )
	{
		ri.Printf( PRINT_DEVELOPER, "R_LoadSVG: failed to parse '%s'\n", name );
		return;
	}

	// Get image dimensions
	w = (int)image->width;
	h = (int)image->height;

	if ( w <= 0 || h <= 0 )
	{
		nsvgDelete( image );
		ri.Printf( PRINT_DEVELOPER, "R_LoadSVG: invalid dimensions for '%s'\n", name );
		return;
	}

	// Allocate rasterization buffer
	img = (unsigned char *)ri.Malloc( w * h * 4 );
	if ( !img )
	{
		nsvgDelete( image );
		return;
	}

	// Create rasterizer
	rast = nsvgCreateRasterizer();
	if ( !rast )
	{
		ri.Free( img );
		nsvgDelete( image );
		ri.Printf( PRINT_DEVELOPER, "R_LoadSVG: failed to create rasterizer for '%s'\n", name );
		return;
	}

	// Rasterize SVG to RGBA image
	nsvgRasterize( rast, image, 0, 0, 1.0f, img, w, h, w * 4 );

	// Cleanup
	nsvgDeleteRasterizer( rast );
	nsvgDelete( image );

	// Set output values
	*pic = (byte *)img;
	if ( width )
		*width = w;
	if ( height )
		*height = h;
}

#else // !USE_NANOSVG

// Stub implementation when nanosvg support is not compiled in.
void R_LoadSVG( const char *name, byte **pic, int *width, int *height )
{
	*pic = NULL;
	if ( width )
		*width = 0;
	if ( height )
		*height = 0;
	ri.Printf( PRINT_DEVELOPER, "R_LoadSVG: SVG support not available (file '%s')\n", name );
}

#endif // USE_NANOSVG

