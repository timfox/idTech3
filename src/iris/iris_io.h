#pragma once

#include "qcommon/q_shared.h"

/* Engine-native .iris tile atlas container (RGBA8 + optional tile state). */

#define IRIS_FILE_MAGIC      0x53595249u /* 'IRIS' little-endian */
#define IRIS_FILE_VERSION    1u
#define IRIS_FILE_VERSION_V2 2u

typedef struct {
	uint32_t magic;
	uint32_t version;
	uint32_t tile_px;
	uint32_t tiles_x;
	uint32_t tiles_y;
	uint32_t atlas_w;
	uint32_t atlas_h;
	uint32_t payload_bytes;
	uint32_t state_bytes;
} iris_file_header_t;

qboolean Iris_ValidateHeader( const iris_file_header_t *hdr );

qboolean Iris_ParseAtlasBuffer( const uint8_t *buf, int len,
	iris_file_header_t *hdr_out, const uint8_t **atlas_out, const uint8_t **state_out );

int Iris_SerializeAtlas( const uint8_t *rgba, const uint32_t *tile_state,
	uint32_t atlas_w, uint32_t atlas_h,
	uint32_t tile_px, uint32_t tiles_x, uint32_t tiles_y,
	uint8_t *out, int out_cap );

const char *Iris_IoErrorString( int err );
int Iris_IoLastError( void );
#define IRIS_IO_OK              0
#define IRIS_IO_ERR_NULL        1
#define IRIS_IO_ERR_SHORT       2
#define IRIS_IO_ERR_MAGIC       3
#define IRIS_IO_ERR_VERSION     4
#define IRIS_IO_ERR_DIM         5
#define IRIS_IO_ERR_SIZE        6
#define IRIS_IO_ERR_BUFFER      7
