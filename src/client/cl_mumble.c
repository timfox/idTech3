/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Mumble positional audio link via shared memory.

Mumble uses a shared memory region to receive player position data
from game clients. When enabled, this module maps the "MumbleLink"
shared memory and writes position/orientation each frame.

See: https://wiki.mumble.info/wiki/Link
===========================================================================
*/

#include "client.h"
#include "cl_mumble.h"

#ifdef __linux__
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <wchar.h>

typedef struct {
	uint32_t uiVersion;
	uint32_t uiTick;
	float    fAvatarPosition[3];
	float    fAvatarFront[3];
	float    fAvatarTop[3];
	wchar_t  name[256];
	float    fCameraPosition[3];
	float    fCameraFront[3];
	float    fCameraTop[3];
	wchar_t  identity[256];
	uint32_t context_len;
	unsigned char context[256];
	wchar_t  description[2048];
} LinkedMem;

static LinkedMem *lm = NULL;
static int shmFd = -1;
static cvar_t *cl_mumble;
static cvar_t *cl_mumbleScale;
static uint32_t mumbleTick = 0;

qboolean CL_Mumble_Init( void ) {
	cl_mumble = Cvar_Get( "cl_mumble", "0", CVAR_ARCHIVE );
	cl_mumbleScale = Cvar_Get( "cl_mumbleScale", "0.0254", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_mumble, "Enable Mumble positional audio link (0 = off, 1 = on)." );
	Cvar_SetDescription( cl_mumbleScale, "Scale factor for position data sent to Mumble (Q3 units to meters)." );

	if ( !cl_mumble->integer ) {
		Com_Printf( "Mumble link: disabled (cl_mumble 0)\n" );
		return qfalse;
	}

	shmFd = shm_open( "/MumbleLink", O_RDWR, S_IRUSR | S_IWUSR );
	if ( shmFd < 0 ) {
		Com_Printf( "Mumble link: shared memory not found (is Mumble running?)\n" );
		return qfalse;
	}

	lm = (LinkedMem *)mmap( NULL, sizeof( LinkedMem ), PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0 );
	if ( lm == MAP_FAILED ) {
		Com_Printf( S_COLOR_YELLOW "Mumble link: mmap failed\n" );
		close( shmFd );
		shmFd = -1;
		lm = NULL;
		return qfalse;
	}

	memset( lm, 0, sizeof( LinkedMem ) );
	lm->uiVersion = 2;
	mbstowcs( lm->name, "idTech3", 256 );
	mbstowcs( lm->description, "id Tech 3 Engine", 2048 );

	Com_Printf( "Mumble link: connected (scale %.4f)\n", cl_mumbleScale->value );
	return qtrue;
}

void CL_Mumble_Shutdown( void ) {
	if ( lm ) {
		munmap( lm, sizeof( LinkedMem ) );
		lm = NULL;
	}
	if ( shmFd >= 0 ) {
		close( shmFd );
		shmFd = -1;
	}
}

void CL_Mumble_Update( const vec3_t position, const vec3_t forward, const vec3_t up ) {
	float scale;

	if ( !lm || !cl_mumble || !cl_mumble->integer ) return;

	scale = cl_mumbleScale->value;

	lm->uiVersion = 2;
	lm->uiTick = ++mumbleTick;

	lm->fAvatarPosition[0] = position[0] * scale;
	lm->fAvatarPosition[1] = position[2] * scale;
	lm->fAvatarPosition[2] = position[1] * scale;

	lm->fAvatarFront[0] = forward[0];
	lm->fAvatarFront[1] = forward[2];
	lm->fAvatarFront[2] = forward[1];

	lm->fAvatarTop[0] = up[0];
	lm->fAvatarTop[1] = up[2];
	lm->fAvatarTop[2] = up[1];

	lm->fCameraPosition[0] = position[0] * scale;
	lm->fCameraPosition[1] = position[2] * scale;
	lm->fCameraPosition[2] = position[1] * scale;

	lm->fCameraFront[0] = forward[0];
	lm->fCameraFront[1] = forward[2];
	lm->fCameraFront[2] = forward[1];

	lm->fCameraTop[0] = up[0];
	lm->fCameraTop[1] = up[2];
	lm->fCameraTop[2] = up[1];
}

#elif defined(_WIN32)

#include <windows.h>
#include <string.h>

typedef struct {
	UINT32   uiVersion;
	DWORD    uiTick;
	float    fAvatarPosition[3];
	float    fAvatarFront[3];
	float    fAvatarTop[3];
	wchar_t  name[256];
	float    fCameraPosition[3];
	float    fCameraFront[3];
	float    fCameraTop[3];
	wchar_t  identity[256];
	UINT32   context_len;
	unsigned char context[256];
	wchar_t  description[2048];
} LinkedMem;

static LinkedMem *lm = NULL;
static HANDLE hMap = NULL;
static cvar_t *cl_mumble;
static cvar_t *cl_mumbleScale;
static DWORD mumbleTick = 0;

qboolean CL_Mumble_Init( void ) {
	cl_mumble = Cvar_Get( "cl_mumble", "0", CVAR_ARCHIVE );
	cl_mumbleScale = Cvar_Get( "cl_mumbleScale", "0.0254", CVAR_ARCHIVE );

	if ( !cl_mumble->integer ) {
		Com_Printf( "Mumble link: disabled (cl_mumble 0)\n" );
		return qfalse;
	}

	hMap = OpenFileMappingW( FILE_MAP_ALL_ACCESS, FALSE, L"MumbleLink" );
	if ( !hMap ) {
		Com_Printf( "Mumble link: shared memory not found (is Mumble running?)\n" );
		return qfalse;
	}

	lm = (LinkedMem *)MapViewOfFile( hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof( LinkedMem ) );
	if ( !lm ) {
		CloseHandle( hMap );
		hMap = NULL;
		return qfalse;
	}

	memset( lm, 0, sizeof( LinkedMem ) );
	lm->uiVersion = 2;
	wcscpy_s( lm->name, 256, L"idTech3" );

	Com_Printf( "Mumble link: connected\n" );
	return qtrue;
}

void CL_Mumble_Shutdown( void ) {
	if ( lm ) { UnmapViewOfFile( lm ); lm = NULL; }
	if ( hMap ) { CloseHandle( hMap ); hMap = NULL; }
}

void CL_Mumble_Update( const vec3_t position, const vec3_t forward, const vec3_t up ) {
	float scale;
	if ( !lm || !cl_mumble || !cl_mumble->integer ) return;
	scale = cl_mumbleScale->value;
	lm->uiVersion = 2;
	lm->uiTick = ++mumbleTick;
	lm->fAvatarPosition[0] = position[0] * scale;
	lm->fAvatarPosition[1] = position[2] * scale;
	lm->fAvatarPosition[2] = position[1] * scale;
	lm->fAvatarFront[0] = forward[0]; lm->fAvatarFront[1] = forward[2]; lm->fAvatarFront[2] = forward[1];
	lm->fAvatarTop[0] = up[0]; lm->fAvatarTop[1] = up[2]; lm->fAvatarTop[2] = up[1];
	lm->fCameraPosition[0] = position[0] * scale;
	lm->fCameraPosition[1] = position[2] * scale;
	lm->fCameraPosition[2] = position[1] * scale;
	lm->fCameraFront[0] = forward[0]; lm->fCameraFront[1] = forward[2]; lm->fCameraFront[2] = forward[1];
	lm->fCameraTop[0] = up[0]; lm->fCameraTop[1] = up[2]; lm->fCameraTop[2] = up[1];
}

#else
/* Unsupported platform */
qboolean CL_Mumble_Init( void ) { Com_Printf( "Mumble link: not supported on this platform\n" ); return qfalse; }
void CL_Mumble_Shutdown( void ) {}
void CL_Mumble_Update( const vec3_t position, const vec3_t forward, const vec3_t up ) { (void)position; (void)forward; (void)up; }
#endif
