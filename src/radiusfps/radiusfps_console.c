/*
===========================================================================
RadiusFPS console commands and cvars.
===========================================================================
*/

#include "radiusfps/radiusfps_console.h"
#include "radiusfps/radiusfps.h"

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static cvar_t *cl_radiusfps_enable;
static cvar_t *cl_radiusfps_nvox;
static cvar_t *cl_radiusfps_backend;
static cvar_t *cl_radiusfps_seed;
static qboolean radiusfps_console_registered = qfalse;

static radiusfps_backend_t RadiusFPS_ParseBackend( const char *name )
{
	if ( !name || !name[0] ) {
		return RADIUSFPS_BACKEND_CPU;
	}
	if ( !Q_stricmp( name, "ref" ) || !Q_stricmp( name, "reference" ) ) {
		return RADIUSFPS_BACKEND_REFERENCE;
	}
	if ( !Q_stricmp( name, "gpu" ) || !Q_stricmp( name, "g" ) || !Q_stricmp( name, "cuda" ) ) {
		return RADIUSFPS_BACKEND_GPU;
	}
	return RADIUSFPS_BACKEND_CPU;
}

static void RadiusFPS_FillGrid( float *pts, int n, float scale )
{
	int i;
	const float step = scale / (float)( n > 1 ? n - 1 : 1 );

	for ( i = 0; i < n; i++ ) {
		const float t = step * (float)i;
		pts[i * 3 + 0] = t;
		pts[i * 3 + 1] = sinf( t * 0.37f ) * scale * 0.1f;
		pts[i * 3 + 2] = cosf( t * 0.19f ) * scale * 0.1f;
	}
}

static void RadiusFPS_Cmd_Sample_f( void )
{
	radiusfps_config_t cfg;
	int n;
	int m;
	float *pts;
	int *idx_ref;
	int *idx_test;
	radiusfps_profile_t prof;
	int i;
	clock_t t0;
	clock_t t1;
	double ms_ref;
	double ms_test;
	qboolean match;

	if ( !cl_radiusfps_enable || !cl_radiusfps_enable->integer ) {
		Com_Printf( "[RadiusFPS] disabled (cl_radiusfps_enable 0)\n" );
		return;
	}

	n = ( Cmd_Argc() >= 2 ) ? atoi( Cmd_Argv( 1 ) ) : 4096;
	m = ( Cmd_Argc() >= 3 ) ? atoi( Cmd_Argv( 2 ) ) : n / 4;
	if ( n < 8 ) {
		n = 8;
	}
	if ( m < 2 ) {
		m = 2;
	}
	if ( m > n ) {
		m = n;
	}

	RadiusFPS_DefaultConfig( &cfg );
	cfg.nvox = cl_radiusfps_nvox ? cl_radiusfps_nvox->integer : RADIUSFPS_DEFAULT_NVOX;
	cfg.seed = cl_radiusfps_seed ? (unsigned int)cl_radiusfps_seed->integer : 1u;
	cfg.backend = RadiusFPS_ParseBackend( cl_radiusfps_backend ? cl_radiusfps_backend->string : "cpu" );
	if ( cfg.nvox < 4 ) {
		cfg.nvox = 4;
	}

	pts = (float *)Z_Malloc( (size_t)n * 3 * sizeof( float ) );
	idx_ref = (int *)Z_Malloc( (size_t)m * sizeof( int ) );
	idx_test = (int *)Z_Malloc( (size_t)m * sizeof( int ) );
	if ( !pts || !idx_ref || !idx_test ) {
		Com_Printf( S_COLOR_RED "[RadiusFPS] out of memory\n" );
		Z_Free( pts );
		Z_Free( idx_ref );
		Z_Free( idx_test );
		return;
	}

	RadiusFPS_FillGrid( pts, n, 10.0f );

	cfg.backend = RADIUSFPS_BACKEND_REFERENCE;
	t0 = clock();
	if ( !RadiusFPS_Sample( pts, n, m, idx_ref, &cfg, NULL, NULL ) ) {
		Com_Printf( S_COLOR_RED "[RadiusFPS] reference sample failed\n" );
		goto done;
	}
	t1 = clock();
	ms_ref = 1000.0 * (double)( t1 - t0 ) / (double)CLOCKS_PER_SEC;

	cfg.backend = RadiusFPS_ParseBackend( cl_radiusfps_backend ? cl_radiusfps_backend->string : "cpu" );
	if ( cfg.backend == RADIUSFPS_BACKEND_GPU && !RadiusFPS_GpuAvailable() ) {
		Com_Printf( S_COLOR_YELLOW "[RadiusFPS] GPU unavailable — using CPU RadiusFPS\n" );
		cfg.backend = RADIUSFPS_BACKEND_CPU;
	}

	t0 = clock();
	if ( !RadiusFPS_Sample( pts, n, m, idx_test, &cfg, NULL, &prof ) ) {
		Com_Printf( S_COLOR_RED "[RadiusFPS] sample failed (%s)\n", RadiusFPS_BackendName( cfg.backend ) );
		goto done;
	}
	t1 = clock();
	ms_test = prof.total_ms > 0.0 ? prof.total_ms : ( 1000.0 * (double)( t1 - t0 ) / (double)CLOCKS_PER_SEC );

	match = qtrue;
	for ( i = 0; i < m; i++ ) {
		if ( idx_ref[i] != idx_test[i] ) {
			match = qfalse;
			break;
		}
	}

	Com_Printf( "[RadiusFPS] n=%d m=%d nvox=%d seed=%u backend=%s gpu=%s exact=%s\n",
		n, m, cfg.nvox, cfg.seed,
		RadiusFPS_BackendName( cfg.backend ),
		RadiusFPS_GpuAvailable() ? "yes" : "no",
		match ? "yes" : "NO" );
	Com_Printf( "[RadiusFPS] reference %.2f ms | %s %.2f ms (pre %.2f init %.2f iter %.2f) | speedup %.2fx\n",
		ms_ref, RadiusFPS_BackendName( cfg.backend ), ms_test,
		prof.preprocess_ms, prof.init_ms, prof.iterate_ms,
		ms_test > 0.0 ? ms_ref / ms_test : 0.0 );

done:
	Z_Free( pts );
	Z_Free( idx_ref );
	Z_Free( idx_test );
}

static void RadiusFPS_Cmd_Info_f( void )
{
	Com_Printf( "[RadiusFPS] Efficient farthest point sampling (Yu et al., arXiv:2606.06255)\n" );
	Com_Printf( "[RadiusFPS] Spherical voxel radius pruning + coordinate-wise point skip\n" );
	Com_Printf( "[RadiusFPS] cvars: cl_radiusfps_enable cl_radiusfps_nvox cl_radiusfps_backend cl_radiusfps_seed\n" );
	Com_Printf( "[RadiusFPS] commands: radiusfps_sample [points] [samples] | radiusfps_info\n" );
	Com_Printf( "[RadiusFPS] backends: cpu | gpu (RadiusFPS-G) | ref\n" );
	Com_Printf( "[RadiusFPS] CUDA build: %s | device: %s\n",
#ifdef RADIUSFPS_HAVE_CUDA
		"yes",
#else
		"no",
#endif
		RadiusFPS_GpuAvailable() ? "ready" : "unavailable" );
}

void RadiusFPS_ConsoleInit( void )
{
	if ( radiusfps_console_registered ) {
		return;
	}

	cl_radiusfps_enable = Cvar_Get( "cl_radiusfps_enable", "1", CVAR_ARCHIVE );
	cl_radiusfps_nvox = Cvar_Get( "cl_radiusfps_nvox", "16", CVAR_ARCHIVE );
	cl_radiusfps_backend = Cvar_Get( "cl_radiusfps_backend", "cpu", CVAR_ARCHIVE );
	cl_radiusfps_seed = Cvar_Get( "cl_radiusfps_seed", "1", CVAR_ARCHIVE );

	Cvar_SetDescription( cl_radiusfps_enable,
		"Enable RadiusFPS farthest point sampling console self-test (startup log when 1)." );
	Cvar_SetDescription( cl_radiusfps_nvox,
		"Voxel bins per axis for spherical voxel pruning (paper parameter v, default 16)." );
	Cvar_SetDescription( cl_radiusfps_backend,
		"RadiusFPS backend: cpu, gpu (RadiusFPS-G), or ref (vanilla FPS baseline)." );
	Cvar_SetDescription( cl_radiusfps_seed,
		"Deterministic RNG seed for initial FPS sample point." );

	Cmd_AddCommand( "radiusfps_sample", RadiusFPS_Cmd_Sample_f );
	Cmd_AddCommand( "radiusfps_info", RadiusFPS_Cmd_Info_f );

	radiusfps_console_registered = qtrue;

	if ( cl_radiusfps_enable->integer ) {
		Com_Printf( "[RadiusFPS] enabled backend=%s nvox=%d cuda=%s\n",
			cl_radiusfps_backend->string,
			cl_radiusfps_nvox->integer,
			RadiusFPS_GpuAvailable() ? "yes" : "no" );
	}
}
