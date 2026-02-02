/*
===========================================================================
libpng-based PNG decoder for idTech3
===========================================================================
*/

#include "../../qcommon/q_shared.h"
#include "../../qcommon/qcommon.h"
#include "tr_image_loaders.h"
#include "tr_fs_compat.h"
#include "tr_public.h"

#include <png.h>

#define Q3IMAGE_BYTESPERPIXEL (4)

typedef struct {
	fileHandle_t file;
} png_io_t;

static void PNG_ReadCallback( png_structp png_ptr, png_bytep data, png_size_t length )
{
	png_io_t *io = (png_io_t *)png_get_io_ptr( png_ptr );
	int bytesRead;

	if ( !io || !data || length == 0 ) {
		png_error( png_ptr, "PNG_ReadCallback invalid parameters" );
		return;
	}

	bytesRead = FS_Read( data, (int)length, io->file );
	if ( bytesRead != (int)length ) {
		png_error( png_ptr, "PNG_ReadCallback short read" );
	}
}

void R_LoadPNG( const char *name, byte **pic, int *width, int *height )
{
	fileHandle_t f;
	int length;
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	png_io_t io;
	png_bytep *row_ptrs = NULL;
	byte *image = NULL;
	png_uint_32 imgWidth = 0;
	png_uint_32 imgHeight = 0;
	int bit_depth = 0;
	int color_type = 0;
	int has_trns = 0;
	int y;
	byte signature[8];

	if ( !name || !pic ) {
		return;
	}

	*pic = NULL;
	if ( width ) {
		*width = 0;
	}
	if ( height ) {
		*height = 0;
	}

	length = FS_FOpenFileRead( name, &f, qtrue );
	if ( f == FS_INVALID_HANDLE || length <= 0 ) {
		return;
	}

	if ( FS_Read( signature, (int)sizeof( signature ), f ) != (int)sizeof( signature ) ) {
		FS_FCloseFile( f );
		return;
	}

	if ( png_sig_cmp( signature, 0, sizeof( signature ) ) ) {
		FS_FCloseFile( f );
		return;
	}

	png_ptr = png_create_read_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL );
	if ( !png_ptr ) {
		FS_FCloseFile( f );
		return;
	}

	info_ptr = png_create_info_struct( png_ptr );
	if ( !info_ptr ) {
		png_destroy_read_struct( &png_ptr, NULL, NULL );
		FS_FCloseFile( f );
		return;
	}

	if ( setjmp( png_jmpbuf( png_ptr ) ) ) {
		if ( row_ptrs ) {
			ri.Free( row_ptrs );
		}
		if ( image ) {
			ri.Free( image );
		}
		png_destroy_read_struct( &png_ptr, &info_ptr, NULL );
		FS_FCloseFile( f );
		return;
	}

	io.file = f;
	png_set_read_fn( png_ptr, &io, PNG_ReadCallback );
	png_set_sig_bytes( png_ptr, sizeof( signature ) );

	png_read_info( png_ptr, info_ptr );

	imgWidth = png_get_image_width( png_ptr, info_ptr );
	imgHeight = png_get_image_height( png_ptr, info_ptr );
	color_type = png_get_color_type( png_ptr, info_ptr );
	bit_depth = png_get_bit_depth( png_ptr, info_ptr );
	has_trns = png_get_valid( png_ptr, info_ptr, PNG_INFO_tRNS );

	if ( bit_depth == 16 ) {
		png_set_strip_16( png_ptr );
	}
	if ( color_type == PNG_COLOR_TYPE_PALETTE ) {
		png_set_palette_to_rgb( png_ptr );
	}
	if ( color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8 ) {
		png_set_expand_gray_1_2_4_to_8( png_ptr );
	}
	if ( has_trns ) {
		png_set_tRNS_to_alpha( png_ptr );
	}
	if ( color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA ) {
		png_set_gray_to_rgb( png_ptr );
	}
	if ( !( color_type & PNG_COLOR_MASK_ALPHA ) && !has_trns ) {
		png_set_add_alpha( png_ptr, 0xFF, PNG_FILLER_AFTER );
	}

	png_read_update_info( png_ptr, info_ptr );

	if ( imgWidth == 0 || imgHeight == 0 ) {
		png_error( png_ptr, "PNG has invalid dimensions" );
	}

	image = (byte *)ri.Malloc( imgWidth * imgHeight * Q3IMAGE_BYTESPERPIXEL );
	row_ptrs = (png_bytep *)ri.Malloc( imgHeight * sizeof( png_bytep ) );

	for ( y = 0; y < (int)imgHeight; y++ ) {
		row_ptrs[y] = image + ( y * imgWidth * Q3IMAGE_BYTESPERPIXEL );
	}

	png_read_image( png_ptr, row_ptrs );
	png_read_end( png_ptr, NULL );

	png_destroy_read_struct( &png_ptr, &info_ptr, NULL );
	FS_FCloseFile( f );

	ri.Free( row_ptrs );

	*pic = image;
	if ( width ) {
		*width = (int)imgWidth;
	}
	if ( height ) {
		*height = (int)imgHeight;
	}
}
