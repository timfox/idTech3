/*
 * Guest keyboard/mouse input ring — POSIX shm written by engine, read by QEMU fork.
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

static qboolean s_captureKeys;
static qboolean s_shmReady;
static uint32_t s_eventsDropped;

#ifndef _WIN32
static int s_shmFd = -1;
static byte *s_shmBase;
static size_t s_shmSize;
static emulatorInputHeader_t *s_hdr;
static emulatorInputEvent_t *s_ring;
#endif

static uint32_t Emulator_Input_Modifiers( void )
{
	uint32_t mods = 0;

	if ( keys[K_SHIFT].down ) {
		mods |= 1u;
	}
	if ( keys[K_CTRL].down ) {
		mods |= 2u;
	}
	if ( keys[K_ALT].down ) {
		mods |= 4u;
	}
	return mods;
}

#ifndef _WIN32
static qboolean Emulator_Input_EnsureShm( void )
{
	size_t need;

	if ( s_shmReady ) {
		return qtrue;
	}

	need = sizeof( emulatorInputHeader_t ) +
		(size_t)EMULATOR_INPUT_RING * sizeof( emulatorInputEvent_t );

	s_shmFd = shm_open( EMULATOR_INPUT_SHM_NAME, O_CREAT | O_RDWR, 0600 );
	if ( s_shmFd < 0 ) {
		return qfalse;
	}

	if ( ftruncate( s_shmFd, (off_t)need ) != 0 ) {
		close( s_shmFd );
		s_shmFd = -1;
		return qfalse;
	}

	s_shmSize = need;
	s_shmBase = mmap( NULL, s_shmSize, PROT_READ | PROT_WRITE, MAP_SHARED, s_shmFd, 0 );
	if ( s_shmBase == MAP_FAILED ) {
		close( s_shmFd );
		s_shmFd = -1;
		s_shmBase = NULL;
		return qfalse;
	}

	s_hdr = (emulatorInputHeader_t *)s_shmBase;
	s_ring = (emulatorInputEvent_t *)( s_shmBase + sizeof( *s_hdr ) );
	Com_Memset( s_hdr, 0, sizeof( *s_hdr ) );
	s_hdr->magic = EMULATOR_INPUT_MAGIC;
	s_hdr->ringSize = EMULATOR_INPUT_RING;
	s_shmReady = qtrue;
	Com_Printf( "[emulator] input shm %s (ring=%d)\n", EMULATOR_INPUT_SHM_NAME, EMULATOR_INPUT_RING );
	return qtrue;
}

static void Emulator_Input_DestroyShm( void )
{
	if ( s_shmBase && s_shmBase != MAP_FAILED ) {
		munmap( s_shmBase, s_shmSize );
	}
	if ( s_shmFd >= 0 ) {
		close( s_shmFd );
		shm_unlink( EMULATOR_INPUT_SHM_NAME );
	}
	s_shmBase = NULL;
	s_shmFd = -1;
	s_shmSize = 0;
	s_hdr = NULL;
	s_ring = NULL;
	s_shmReady = qfalse;
}
#endif

void Emulator_Input_Init( void )
{
	s_captureKeys = qfalse;
	s_eventsDropped = 0;
#ifndef _WIN32
	Emulator_Input_EnsureShm();
#endif
}

void Emulator_Input_Shutdown( void )
{
#ifndef _WIN32
	Emulator_Input_DestroyShm();
#endif
	s_captureKeys = qfalse;
}

void Emulator_Input_SetCapture( qboolean capture )
{
	s_captureKeys = capture;
	if ( capture ) {
		Com_Printf( "[emulator] keyboard capture active (ESC releases)\n" );
	} else {
		Com_Printf( "[emulator] keyboard capture released\n" );
	}
}

qboolean Emulator_Input_CaptureActive( void )
{
	return s_captureKeys;
}

qboolean Emulator_Input_Push( emulatorInputType_t type, int key, int ascii )
{
#ifndef _WIN32
	uint32_t w, r, next;
	emulatorInputEvent_t *ev;

	if ( !s_captureKeys || !Cvar_VariableIntegerValue( "cl_emulator" ) ) {
		return qfalse;
	}
	if ( !Emulator_Input_EnsureShm() || !s_hdr || !s_ring ) {
		return qfalse;
	}

	w = s_hdr->writeIdx;
	r = s_hdr->readIdx;
	if ( w - r >= s_hdr->ringSize ) {
		s_eventsDropped++;
		return qfalse;
	}

	next = w + 1;
	ev = &s_ring[w % s_hdr->ringSize];
	ev->type = (uint32_t)type;
	ev->key = (uint32_t)key;
	ev->ascii = (uint32_t)ascii;
	ev->mods = Emulator_Input_Modifiers();
	s_hdr->writeIdx = next;
	return qtrue;
#else
	(void)type;
	(void)key;
	(void)ascii;
	return qfalse;
#endif
}

void Emulator_Input_GetStatus( uint32_t *writeIdx, uint32_t *readIdx, uint32_t *dropped )
{
	if ( writeIdx ) {
#ifndef _WIN32
		*writeIdx = s_hdr ? s_hdr->writeIdx : 0;
#else
		*writeIdx = 0;
#endif
	}
	if ( readIdx ) {
#ifndef _WIN32
		*readIdx = s_hdr ? s_hdr->readIdx : 0;
#else
		*readIdx = 0;
#endif
	}
	if ( dropped ) {
		*dropped = s_eventsDropped;
	}
}
