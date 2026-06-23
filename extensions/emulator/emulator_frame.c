/*
 * Guest display frame pump — POSIX shm when present, animated test pattern otherwise.
 */
#if !defined( _WIN32 ) && !defined( _DEFAULT_SOURCE )
#define _DEFAULT_SOURCE
#endif

#include "emulator_internal.h"

#include "client.h"

#ifndef _WIN32
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

static int s_width = EMULATOR_DEFAULT_WIDTH;
static int s_height = EMULATOR_DEFAULT_HEIGHT;
static uint32_t s_localFrameIndex;
static qboolean s_shmAttached;

#ifndef _WIN32
static int s_shmFd = -1;
static byte *s_shmBase;
static size_t s_shmSize;
#endif

void Emulator_Frame_SetSize( int width, int height )
{
	if ( width >= 64 && width <= EMULATOR_MAX_WIDTH ) {
		s_width = width;
	}
	if ( height >= 64 && height <= EMULATOR_MAX_HEIGHT ) {
		s_height = height;
	}
}

void Emulator_Frame_GetMetrics( int *width, int *height, uint32_t *frameIndex )
{
	if ( width ) {
		*width = s_width;
	}
	if ( height ) {
		*height = s_height;
	}
	if ( frameIndex ) {
		*frameIndex = s_localFrameIndex;
	}
}

static void Emulator_Frame_FillTestPattern( byte *rgba, int width, int height, uint32_t frameIndex )
{
	int x, y;
	const byte phase = (byte)( frameIndex & 255 );

	for ( y = 0; y < height; y++ ) {
		for ( x = 0; x < width; x++ ) {
			byte *px = rgba + ( y * width + x ) * 4;
			px[0] = (byte)( x + phase );
			px[1] = (byte)( y + ( phase >> 1 ) );
			px[2] = (byte)( 128 + ( ( x ^ y ) & 127 ) );
			px[3] = 255;
		}
	}
}

#ifndef _WIN32
static void Emulator_Frame_DetachShm( void );

static qboolean Emulator_Frame_TryAttachShm( int width, int height )
{
	struct stat st;

	(void)width;
	(void)height;

	if ( s_shmAttached ) {
		if ( s_shmFd >= 0 && fstat( s_shmFd, &st ) == 0 && (size_t)st.st_size != s_shmSize ) {
			Emulator_Frame_DetachShm();
		} else {
			return qtrue;
		}
	}

	s_shmFd = shm_open( EMULATOR_SHM_NAME, O_RDONLY, 0 );
	if ( s_shmFd < 0 ) {
		return qfalse;
	}

	if ( fstat( s_shmFd, &st ) != 0 || st.st_size < (off_t)sizeof( emulatorFrameHeader_t ) ) {
		close( s_shmFd );
		s_shmFd = -1;
		return qfalse;
	}

	s_shmSize = (size_t)st.st_size;
	s_shmBase = mmap( NULL, s_shmSize, PROT_READ, MAP_SHARED, s_shmFd, 0 );
	if ( s_shmBase == MAP_FAILED ) {
		close( s_shmFd );
		s_shmFd = -1;
		s_shmBase = NULL;
		s_shmSize = 0;
		return qfalse;
	}

	s_shmAttached = qtrue;
	Com_Printf( "[emulator] attached POSIX shm %s (%zu bytes)\n", EMULATOR_SHM_NAME, s_shmSize );
	return qtrue;
}

static void Emulator_Frame_DetachShm( void )
{
	if ( s_shmBase && s_shmBase != MAP_FAILED ) {
		munmap( s_shmBase, s_shmSize );
	}
	if ( s_shmFd >= 0 ) {
		close( s_shmFd );
	}
	s_shmBase = NULL;
	s_shmFd = -1;
	s_shmSize = 0;
	s_shmAttached = qfalse;
}
#endif

qboolean Emulator_Frame_ShmAttached( void )
{
	return s_shmAttached;
}

void Emulator_Frame_Init( void )
{
	s_width = EMULATOR_DEFAULT_WIDTH;
	s_height = EMULATOR_DEFAULT_HEIGHT;
	s_localFrameIndex = 0;
	s_shmAttached = qfalse;
}

void Emulator_Frame_Shutdown( void )
{
#ifndef _WIN32
	Emulator_Frame_DetachShm();
#endif
}

qboolean Emulator_Frame_Pump( byte *rgbaOut, int rgbaBytes, int *width, int *height )
{
	emulatorStatus_t status;
	int w, h;
	size_t need;

	if ( !rgbaOut || rgbaBytes <= 0 || !width || !height ) {
		return qfalse;
	}

	Emulator_Process_GetStatus( &status );
	w = s_width;
	h = s_height;
	if ( w < 64 || h < 64 || w > EMULATOR_MAX_WIDTH || h > EMULATOR_MAX_HEIGHT ) {
		w = EMULATOR_DEFAULT_WIDTH;
		h = EMULATOR_DEFAULT_HEIGHT;
	}

	need = (size_t)w * (size_t)h * 4u;
	if ( (int)need > rgbaBytes ) {
		return qfalse;
	}

#ifndef _WIN32
	if ( status.guestRunning && Emulator_Frame_TryAttachShm( w, h ) ) {
		const emulatorFrameHeader_t *hdr = (const emulatorFrameHeader_t *)s_shmBase;
		const byte *pixels;

		if ( hdr && hdr->magic == EMULATOR_FRAME_MAGIC && hdr->width > 0 && hdr->height > 0 ) {
			w = (int)hdr->width;
			h = (int)hdr->height;
			need = sizeof( *hdr ) + (size_t)w * (size_t)h * 4u;
			if ( (int)( need - sizeof( *hdr ) ) <= rgbaBytes && s_shmSize >= need ) {
				pixels = s_shmBase + sizeof( *hdr );
				Com_Memcpy( rgbaOut, pixels, need - sizeof( *hdr ) );
				s_localFrameIndex = hdr->frameIndex;
				Emulator_Frame_SetSize( w, h );
				*width = w;
				*height = h;
				return qtrue;
			}
		}
	} else if ( !status.guestRunning && s_shmAttached ) {
		Emulator_Frame_DetachShm();
	}
#endif

	Emulator_Frame_FillTestPattern( rgbaOut, w, h, ++s_localFrameIndex );
	*width = w;
	*height = h;
	return qtrue;
}
