/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Iris .iris tile atlas container I/O.
===========================================================================
*/

#include "iris/iris_io.h"

#include <stddef.h>
#include <string.h>

static int iris_io_last_error = IRIS_IO_OK;

int Iris_IoLastError( void )
{
	return iris_io_last_error;
}

const char *Iris_IoErrorString( int err )
{
	switch ( err ) {
	case IRIS_IO_OK:
		return "ok";
	case IRIS_IO_ERR_NULL:
		return "null argument";
	case IRIS_IO_ERR_SHORT:
		return "buffer too short";
	case IRIS_IO_ERR_MAGIC:
		return "bad magic";
	case IRIS_IO_ERR_VERSION:
		return "unsupported version";
	case IRIS_IO_ERR_DIM:
		return "invalid dimensions";
	case IRIS_IO_ERR_SIZE:
		return "payload size mismatch";
	case IRIS_IO_ERR_BUFFER:
		return "output buffer too small";
	default:
		return "unknown error";
	}
}

static qboolean Iris_ValidateDims( const iris_file_header_t *hdr )
{
	if ( hdr->tile_px < 64u || hdr->tile_px > 512u ||
		hdr->tiles_x < 1u || hdr->tiles_y < 1u ||
		hdr->atlas_w != hdr->tiles_x * hdr->tile_px ||
		hdr->atlas_h != hdr->tiles_y * hdr->tile_px ) {
		iris_io_last_error = IRIS_IO_ERR_DIM;
		return qfalse;
	}
	if ( hdr->payload_bytes != hdr->atlas_w * hdr->atlas_h * 4u ) {
		iris_io_last_error = IRIS_IO_ERR_SIZE;
		return qfalse;
	}
	return qtrue;
}

qboolean Iris_ValidateHeader( const iris_file_header_t *hdr )
{
	if ( !hdr ) {
		iris_io_last_error = IRIS_IO_ERR_NULL;
		return qfalse;
	}
	if ( hdr->magic != IRIS_FILE_MAGIC ) {
		iris_io_last_error = IRIS_IO_ERR_MAGIC;
		return qfalse;
	}
	if ( hdr->version != IRIS_FILE_VERSION && hdr->version != IRIS_FILE_VERSION_V2 ) {
		iris_io_last_error = IRIS_IO_ERR_VERSION;
		return qfalse;
	}
	if ( !Iris_ValidateDims( hdr ) ) {
		return qfalse;
	}
	if ( hdr->version == IRIS_FILE_VERSION ) {
		if ( hdr->state_bytes != 0u ) {
			iris_io_last_error = IRIS_IO_ERR_SIZE;
			return qfalse;
		}
	} else if ( hdr->state_bytes != hdr->tiles_x * hdr->tiles_y * (uint32_t)sizeof( uint32_t ) ) {
		iris_io_last_error = IRIS_IO_ERR_SIZE;
		return qfalse;
	}
	iris_io_last_error = IRIS_IO_OK;
	return qtrue;
}

qboolean Iris_ParseAtlasBuffer( const uint8_t *buf, int len,
	iris_file_header_t *hdr_out, const uint8_t **atlas_out, const uint8_t **state_out )
{
	int minHeader;

	if ( !buf || !hdr_out || !atlas_out ) {
		iris_io_last_error = IRIS_IO_ERR_NULL;
		return qfalse;
	}

	if ( len < 8 ) {
		iris_io_last_error = IRIS_IO_ERR_SHORT;
		return qfalse;
	}

	if ( *(const uint32_t *)buf != IRIS_FILE_MAGIC ) {
		iris_io_last_error = IRIS_IO_ERR_MAGIC;
		return qfalse;
	}

	hdr_out->magic = IRIS_FILE_MAGIC;
	hdr_out->version = *(const uint32_t *)( buf + 4 );
	if ( hdr_out->version == IRIS_FILE_VERSION ) {
		minHeader = (int)( offsetof( iris_file_header_t, state_bytes ) );
		if ( len < minHeader ) {
			iris_io_last_error = IRIS_IO_ERR_SHORT;
			return qfalse;
		}
		Com_Memcpy( hdr_out, buf, (size_t)minHeader );
		hdr_out->state_bytes = 0u;
	} else if ( hdr_out->version == IRIS_FILE_VERSION_V2 ) {
		minHeader = (int)sizeof( iris_file_header_t );
		if ( len < minHeader ) {
			iris_io_last_error = IRIS_IO_ERR_SHORT;
			return qfalse;
		}
		Com_Memcpy( hdr_out, buf, sizeof( *hdr_out ) );
	} else {
		iris_io_last_error = IRIS_IO_ERR_VERSION;
		return qfalse;
	}

	if ( !Iris_ValidateHeader( hdr_out ) ) {
		return qfalse;
	}
	if ( len < minHeader + (int)hdr_out->payload_bytes + (int)hdr_out->state_bytes ) {
		iris_io_last_error = IRIS_IO_ERR_SHORT;
		return qfalse;
	}

	*atlas_out = buf + minHeader;
	if ( state_out ) {
		*state_out = ( hdr_out->state_bytes > 0u ) ?
			( buf + minHeader + hdr_out->payload_bytes ) : NULL;
	}
	iris_io_last_error = IRIS_IO_OK;
	return qtrue;
}

int Iris_SerializeAtlas( const uint8_t *rgba, const uint32_t *tile_state,
	uint32_t atlas_w, uint32_t atlas_h,
	uint32_t tile_px, uint32_t tiles_x, uint32_t tiles_y,
	uint8_t *out, int out_cap )
{
	iris_file_header_t hdr;
	uint32_t state_bytes;
	int total;

	if ( !rgba || !out ) {
		iris_io_last_error = IRIS_IO_ERR_NULL;
		return -1;
	}

	hdr.magic = IRIS_FILE_MAGIC;
	hdr.version = IRIS_FILE_VERSION_V2;
	hdr.tile_px = tile_px;
	hdr.tiles_x = tiles_x;
	hdr.tiles_y = tiles_y;
	hdr.atlas_w = atlas_w;
	hdr.atlas_h = atlas_h;
	hdr.payload_bytes = atlas_w * atlas_h * 4u;
	state_bytes = tiles_x * tiles_y * (uint32_t)sizeof( uint32_t );
	hdr.state_bytes = ( tile_state != NULL ) ? state_bytes : 0u;

	if ( !Iris_ValidateHeader( &hdr ) ) {
		return -1;
	}

	total = (int)( sizeof( hdr ) + hdr.payload_bytes + hdr.state_bytes );
	if ( out_cap < total ) {
		iris_io_last_error = IRIS_IO_ERR_BUFFER;
		return -1;
	}

	Com_Memcpy( out, &hdr, sizeof( hdr ) );
	Com_Memcpy( out + sizeof( hdr ), rgba, hdr.payload_bytes );
	if ( tile_state && hdr.state_bytes > 0u ) {
		Com_Memcpy( out + sizeof( hdr ) + hdr.payload_bytes, tile_state, hdr.state_bytes );
	}
	iris_io_last_error = IRIS_IO_OK;
	return total;
}
