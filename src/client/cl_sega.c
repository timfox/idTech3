/*
===========================================================================
SEGA (Spectral-Energy Guided Attention) — external FLUX hi-res generation hook.
See docs/SEGA.md and external/sega/README.md.
===========================================================================
*/

#include "client.h"
#include "cl_sega.h"
#include "cl_pipeline.h"

#ifdef USE_SEGA

#if USE_SDL
#include <SDL2/SDL_thread.h>
#else
typedef struct SDL_Thread SDL_Thread;
#endif

cvar_t *cl_sega_enable;
cvar_t *cl_sega_async;
cvar_t *cl_sega_auto_view;
cvar_t *cl_sega_repo;
cvar_t *cl_sega_python;
cvar_t *cl_sega_conda;
cvar_t *cl_sega_cmd;
cvar_t *cl_sega_width;
cvar_t *cl_sega_height;
cvar_t *cl_sega_steps;
cvar_t *cl_sega_seed;
cvar_t *cl_sega_checkpoint;
cvar_t *cl_sega_multi_gpu;
cvar_t *cl_sega_timeout;

typedef enum {
	SEGA_JOB_IDLE,
	SEGA_JOB_RUNNING,
	SEGA_JOB_COMPLETED,
	SEGA_JOB_FAILED
} sega_job_status_t;

typedef struct {
	sega_job_status_t status;
	qboolean notified;
	int start_time;
	int timeout_seconds;
	char repo[MAX_OSPATH];
	char prompt[1024];
	char output_full[MAX_OSPATH];
	char output_vfs[MAX_QPATH];
	char checkpoint[256];
	char extra_args[512];
	char error_msg[1024];
	SDL_Thread *thread;
} sega_job_t;

static sega_job_t sega_job;

static const char *CL_SegaDefaultCmd( void ) {
	return "conda run -n %N --no-capture-output %P \"%E/sega_flux_generate.py\" --repo \"%R\" --prompt \"%I\" --output \"%O\" --checkpoint \"%M\" %A";
}

static qboolean CL_SegaFileExists( const char *path ) {
	FILE *f;

	if ( !path || !path[0] ) {
		return qfalse;
	}
	f = fopen( path, "rb" );
	if ( f ) {
		fclose( f );
		return qtrue;
	}
	return qfalse;
}

static void CL_SegaResolvePath( const char *base, const char *in, char *out, size_t out_size ) {
	if ( !in || !in[0] || !out || out_size == 0 ) {
		if ( out && out_size > 0 ) {
			out[0] = '\0';
		}
		return;
	}
	if ( in[0] == '/' || ( in[0] && in[1] == ':' ) ) {
		Q_strncpyz( out, in, out_size );
		return;
	}
	if ( base && base[0] ) {
		Com_sprintf( out, out_size, "%s/%s", base, in );
	} else {
		Q_strncpyz( out, in, out_size );
	}
}

static void CL_SegaEnsureOutputDir( const char *output_full ) {
	char *slash;
	char dir[MAX_OSPATH];
	char mkdir_cmd[1024];

	slash = strrchr( output_full, '/' );
	if ( !slash ) {
		return;
	}
	if ( (size_t)( slash - output_full ) >= sizeof( dir ) ) {
		return;
	}
	memcpy( dir, output_full, (size_t)( slash - output_full ) );
	dir[slash - output_full] = '\0';
	Com_sprintf( mkdir_cmd, sizeof( mkdir_cmd ), "mkdir -p \"%s\"", dir );
	system( mkdir_cmd );
}

static qboolean CL_SegaRunExpanded( const char *tmpl, const cl_pipeline_expand_t *ex, qboolean quiet ) {
	char cmd[8192];

	if ( !CL_PipelineExpandTemplate( cmd, sizeof( cmd ), tmpl, ex ) ) {
		if ( !quiet ) {
			Com_Printf( S_COLOR_RED "SEGA: expanded command too long or bad path characters\n" );
		}
		return qfalse;
	}
	if ( !quiet ) {
		Com_Printf( "SEGA: executing: %s\n", cmd );
	}
	if ( system( cmd ) != 0 ) {
		if ( !quiet ) {
			Com_Printf( S_COLOR_RED "SEGA: shell returned non-zero\n" );
		}
		return qfalse;
	}
	return qtrue;
}

static void CL_SegaBuildExtraArgs( char *out, size_t out_size ) {
	int width;
	int height;
	int steps;
	int seed;

	if ( !out || out_size == 0 ) {
		return;
	}
	out[0] = '\0';
	width = cl_sega_width ? cl_sega_width->integer : 4096;
	height = cl_sega_height ? cl_sega_height->integer : 4096;
	steps = cl_sega_steps ? cl_sega_steps->integer : 28;
	seed = cl_sega_seed ? cl_sega_seed->integer : 0;
	Com_sprintf( out, out_size, "--height %d --width %d --steps %d --seed %d",
		height, width, steps, seed );
	if ( cl_sega_multi_gpu && cl_sega_multi_gpu->integer ) {
		Q_strcat( out, out_size, " --multi-gpu" );
	}
}

static qboolean CL_SegaBuildExpandFromJob( cl_pipeline_expand_t *ex ) {
	const char *base;
	const char *py;
	const char *conda;

	if ( !ex ) {
		return qfalse;
	}
	base = Sys_DefaultBasePath();
	py = ( cl_sega_python && cl_sega_python->string[0] ) ? cl_sega_python->string : "python3";
	conda = ( cl_sega_conda && cl_sega_conda->string[0] ) ? cl_sega_conda->string : "sega";
	Com_Memset( ex, 0, sizeof( *ex ) );
	ex->repo = sega_job.repo;
	ex->base = base;
	ex->engine = base;
	ex->py = py;
	ex->conda = conda;
	ex->image = sega_job.prompt;
	ex->output = sega_job.output_full;
	ex->model = sega_job.checkpoint;
	ex->args = sega_job.extra_args;
	return qtrue;
}

static void CL_SegaJoinThread( void ) {
#if USE_SDL
	if ( sega_job.thread ) {
		int rc = 0;
		SDL_WaitThread( sega_job.thread, &rc );
		sega_job.thread = NULL;
		(void)rc;
	}
#endif
}

static void CL_SegaReloadTexture( const char *vfs_path ) {
	qhandle_t shader;

	if ( !vfs_path || !vfs_path[0] ) {
		return;
	}
	if ( re.ReloadTexture && re.ReloadTexture( vfs_path ) ) {
		Com_Printf( S_COLOR_GREEN "SEGA: texture reloaded: %s\n", vfs_path );
	}
	shader = re.RegisterShader( vfs_path );
	if ( shader ) {
		Com_Printf( S_COLOR_GREEN "SEGA: shader registered: '%s' (handle %d)\n", vfs_path, shader );
	} else {
		Com_Printf( S_COLOR_YELLOW "SEGA: auto-view failed to register shader for %s\n", vfs_path );
	}
}

static void CL_SegaFinalizeOnMain( void ) {
	if ( sega_job.status != SEGA_JOB_COMPLETED ) {
		return;
	}
	Com_Printf( S_COLOR_GREEN "SEGA: wrote %s — sega_view %s\n",
		sega_job.output_vfs, sega_job.output_vfs );
	if ( cl_sega_auto_view && cl_sega_auto_view->integer ) {
		CL_SegaReloadTexture( sega_job.output_vfs );
	}
}

#if USE_SDL
static int CL_SegaGenerationThread( void *data ) {
	sega_job_t *job = (sega_job_t *)data;
	cl_pipeline_expand_t ex;
	const char *tmpl;

	if ( !job ) {
		return -1;
	}
	if ( !CL_SegaBuildExpandFromJob( &ex ) ) {
		Q_strncpyz( job->error_msg, "Failed to build SEGA command", sizeof( job->error_msg ) );
		job->status = SEGA_JOB_FAILED;
		return -1;
	}
	tmpl = ( cl_sega_cmd && cl_sega_cmd->string[0] ) ? cl_sega_cmd->string : CL_SegaDefaultCmd();
	if ( !CL_SegaRunExpanded( tmpl, &ex, qtrue ) ) {
		Q_strncpyz( job->error_msg, "SEGA generation subprocess failed", sizeof( job->error_msg ) );
		job->status = SEGA_JOB_FAILED;
		return -1;
	}
	if ( !CL_SegaFileExists( job->output_full ) ) {
		Q_strncpyz( job->error_msg, "SEGA output PNG missing after subprocess", sizeof( job->error_msg ) );
		job->status = SEGA_JOB_FAILED;
		return -1;
	}
	job->status = SEGA_JOB_COMPLETED;
	return 0;
}
#endif

static qboolean CL_SegaStartJob( const char *prompt ) {
	cl_pipeline_expand_t ex;
	const char *tmpl;
	const char *base;
	const char *repo;
	const char *checkpoint;

	if ( sega_job.status == SEGA_JOB_RUNNING ) {
		Com_Printf( S_COLOR_YELLOW "SEGA: generation already in progress (sega_status / sega_cancel)\n" );
		return qfalse;
	}
	if ( !prompt || !prompt[0] ) {
		Com_Printf( S_COLOR_YELLOW "SEGA: empty prompt\n" );
		return qfalse;
	}
	repo = cl_sega_repo ? cl_sega_repo->string : "";
	if ( !repo || !repo[0] ) {
		Com_Printf( S_COLOR_YELLOW "SEGA: set cl_sega_repo to your SEGA checkout (see docs/SEGA.md)\n" );
		return qfalse;
	}
	base = Sys_DefaultBasePath();
	if ( !base ) {
		Com_Printf( S_COLOR_RED "SEGA: no engine base path\n" );
		return qfalse;
	}
	Com_Memset( &sega_job, 0, sizeof( sega_job ) );
	sega_job.start_time = Com_Milliseconds();
	sega_job.timeout_seconds = cl_sega_timeout ? cl_sega_timeout->integer : 7200;
	Q_strncpyz( sega_job.repo, repo, sizeof( sega_job.repo ) );
	Q_strncpyz( sega_job.prompt, prompt, sizeof( sega_job.prompt ) );
	checkpoint = ( cl_sega_checkpoint && cl_sega_checkpoint->string[0] ) ?
		cl_sega_checkpoint->string : "Krea-dev";
	Q_strncpyz( sega_job.checkpoint, checkpoint, sizeof( sega_job.checkpoint ) );
	CL_SegaBuildExtraArgs( sega_job.extra_args, sizeof( sega_job.extra_args ) );
	Com_sprintf( sega_job.output_vfs, sizeof( sega_job.output_vfs ),
		"screenshots/sega/sega_%d.png", Com_Milliseconds() );
	CL_SegaResolvePath( base, sega_job.output_vfs, sega_job.output_full, sizeof( sega_job.output_full ) );
	CL_SegaEnsureOutputDir( sega_job.output_full );

	if ( cl_sega_async && cl_sega_async->integer ) {
#if USE_SDL
		sega_job.status = SEGA_JOB_RUNNING;
		sega_job.thread = SDL_CreateThread( CL_SegaGenerationThread, "SEGA_Generation", &sega_job );
		if ( !sega_job.thread ) {
			sega_job.status = SEGA_JOB_IDLE;
			Com_Printf( S_COLOR_RED "SEGA: failed to create background thread\n" );
			return qfalse;
		}
		Com_Printf( "SEGA: started background generation -> %s\n", sega_job.output_vfs );
		Com_Printf( "SEGA: Use sega_status / sega_cancel; sega_view when complete\n" );
		return qtrue;
#else
		Com_Printf( S_COLOR_YELLOW "SEGA: async requires SDL threads; running synchronously\n" );
#endif
	}

	sega_job.status = SEGA_JOB_RUNNING;
	if ( !CL_SegaBuildExpandFromJob( &ex ) ) {
		sega_job.status = SEGA_JOB_IDLE;
		return qfalse;
	}
	tmpl = ( cl_sega_cmd && cl_sega_cmd->string[0] ) ? cl_sega_cmd->string : CL_SegaDefaultCmd();
	if ( !CL_SegaRunExpanded( tmpl, &ex, qfalse ) ||
		!CL_SegaFileExists( sega_job.output_full ) ) {
		Q_strncpyz( sega_job.error_msg, "SEGA generation failed", sizeof( sega_job.error_msg ) );
		sega_job.status = SEGA_JOB_FAILED;
		Com_Printf( S_COLOR_RED "SEGA: generation failed\n" );
		return qfalse;
	}
	sega_job.status = SEGA_JOB_COMPLETED;
	CL_SegaFinalizeOnMain();
	sega_job.notified = qtrue;
	return qtrue;
}

void CL_SegaFrame( void ) {
	if ( sega_job.status == SEGA_JOB_RUNNING && sega_job.timeout_seconds > 0 ) {
		int runtime = ( Com_Milliseconds() - sega_job.start_time ) / 1000;
		if ( runtime > sega_job.timeout_seconds ) {
			Com_Printf( S_COLOR_YELLOW "SEGA: generation exceeded %d s (still running); sega_cancel to stop\n",
				sega_job.timeout_seconds );
			sega_job.timeout_seconds = 0;
		}
		return;
	}
	if ( sega_job.status != SEGA_JOB_COMPLETED && sega_job.status != SEGA_JOB_FAILED ) {
		return;
	}
	if ( sega_job.notified ) {
		return;
	}
	CL_SegaJoinThread();
	sega_job.notified = qtrue;
	if ( sega_job.status == SEGA_JOB_COMPLETED ) {
		CL_SegaFinalizeOnMain();
	} else {
		Com_Printf( S_COLOR_RED "SEGA: generation failed: %s\n", sega_job.error_msg );
	}
}

void CL_SegaInit( void ) {
	cl_sega_enable = Cvar_Get( "cl_sega_enable", "0", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_sega_enable, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_sega_enable,
		"Enable SEGA hi-res FLUX generation (training-free attention rescaling). See docs/SEGA.md." );

	cl_sega_async = Cvar_Get( "cl_sega_async", "1", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_sega_async, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_sega_async,
		"SEGA mode: 0=blocking console, 1=background SDL thread (recommended)." );

	cl_sega_auto_view = Cvar_Get( "cl_sega_auto_view", "0", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_sega_auto_view, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_sega_auto_view,
		"When 1, reload/register the output PNG when generation completes." );

	cl_sega_timeout = Cvar_Get( "cl_sega_timeout", "7200", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_sega_timeout, "60", "86400", CV_INTEGER );
	Cvar_SetDescription( cl_sega_timeout, "Warn after this many seconds if a background SEGA job is still running." );

	cl_sega_repo = Cvar_Get( "cl_sega_repo", "", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_sega_repo,
		"Absolute path to a SEGA git checkout (%%R in cl_sega_cmd / sega_generate)." );

	cl_sega_python = Cvar_Get( "cl_sega_python", "python3", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_sega_python, "Python interpreter (%%P) when not using conda run." );

	cl_sega_conda = Cvar_Get( "cl_sega_conda", "sega", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_sega_conda, "Conda environment name (%%N) for default sega_generate command." );

	cl_sega_cmd = Cvar_Get( "cl_sega_cmd", "", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_sega_cmd,
		"Optional shell template for sega_generate. Tokens: %%R repo, %%B base, %%E release dir, %%P python, %%N conda, %%I prompt, %%O output png, %%M checkpoint, %%A extra args." );

	cl_sega_width = Cvar_Get( "cl_sega_width", "4096", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_sega_width, "512", "8192", CV_INTEGER );
	Cvar_SetDescription( cl_sega_width, "Output width in pixels (passed via %%A)." );

	cl_sega_height = Cvar_Get( "cl_sega_height", "4096", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_sega_height, "512", "8192", CV_INTEGER );
	Cvar_SetDescription( cl_sega_height, "Output height in pixels (passed via %%A)." );

	cl_sega_steps = Cvar_Get( "cl_sega_steps", "28", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_sega_steps, "1", "100", CV_INTEGER );
	Cvar_SetDescription( cl_sega_steps, "Denoising steps for SEGA FLUX (passed via %%A)." );

	cl_sega_seed = Cvar_Get( "cl_sega_seed", "0", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_sega_seed, "0", "2147483647", CV_INTEGER );
	Cvar_SetDescription( cl_sega_seed, "Random seed for reproducible generation (passed via %%A)." );

	cl_sega_checkpoint = Cvar_Get( "cl_sega_checkpoint", "Krea-dev", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_sega_checkpoint,
		"FLUX checkpoint shorthand (dev, Krea-dev) or full HF repo id (%%M)." );

	cl_sega_multi_gpu = Cvar_Get( "cl_sega_multi_gpu", "0", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_sega_multi_gpu, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_sega_multi_gpu,
		"When 1, pass --multi-gpu to the wrapper (requires 2+ visible CUDA devices)." );

	Cmd_AddCommand( "sega_generate", CL_SegaGenerate_f );
	Cmd_AddCommand( "sega_status", CL_SegaStatus_f );
	Cmd_AddCommand( "sega_cancel", CL_SegaCancel_f );
	Cmd_AddCommand( "sega_view", CL_SegaView_f );

	Com_Memset( &sega_job, 0, sizeof( sega_job ) );

	if ( cl_sega_enable && cl_sega_enable->integer ) {
		Com_Printf( "SEGA hi-res FLUX: enabled (repo: %s, %dx%d, async: %s)\n",
			( cl_sega_repo && cl_sega_repo->string[0] ) ? cl_sega_repo->string : "unset",
			cl_sega_width ? cl_sega_width->integer : 4096,
			cl_sega_height ? cl_sega_height->integer : 4096,
			( cl_sega_async && cl_sega_async->integer ) ? "on" : "off" );
	} else {
		Com_Printf( "SEGA hi-res FLUX: disabled (cl_sega_enable 0; docs/SEGA.md)\n" );
	}
}

void CL_SegaShutdown( void ) {
#if USE_SDL
	if ( sega_job.thread ) {
		SDL_WaitThread( sega_job.thread, NULL );
		sega_job.thread = NULL;
	}
#endif
	sega_job.status = SEGA_JOB_IDLE;
	Cmd_RemoveCommand( "sega_generate" );
	Cmd_RemoveCommand( "sega_status" );
	Cmd_RemoveCommand( "sega_cancel" );
	Cmd_RemoveCommand( "sega_view" );
}

void CL_SegaGenerate_f( void ) {
	const char *prompt;

	if ( !cl_sega_enable || !cl_sega_enable->integer ) {
		Com_Printf( S_COLOR_YELLOW "sega_generate: set cl_sega_enable 1 (see docs/SEGA.md)\n" );
		return;
	}
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( S_COLOR_YELLOW "Usage: sega_generate <prompt>\n" );
		Com_Printf( "  Example: sega_generate \"a misty mountain at dawn\"\n" );
		return;
	}
	prompt = Cmd_ArgsFrom( 1 );
	CL_SegaStartJob( prompt );
}

void CL_SegaStatus_f( void ) {
	switch ( sega_job.status ) {
	case SEGA_JOB_IDLE:
		Com_Printf( "SEGA: no generation in progress\n" );
		break;
	case SEGA_JOB_RUNNING: {
		int runtime = ( Com_Milliseconds() - sega_job.start_time ) / 1000;
		Com_Printf( "SEGA: generation in progress (%d s)\n", runtime );
		Com_Printf( "SEGA: prompt: %s\n", sega_job.prompt );
		Com_Printf( "SEGA: output: %s\n", sega_job.output_vfs );
		break;
	}
	case SEGA_JOB_COMPLETED:
		Com_Printf( S_COLOR_GREEN "SEGA: complete: %s\n", sega_job.output_vfs );
		Com_Printf( "SEGA: sega_view to hot-reload texture\n" );
		break;
	case SEGA_JOB_FAILED:
		Com_Printf( S_COLOR_RED "SEGA: failed: %s\n", sega_job.error_msg );
		break;
	}
}

void CL_SegaCancel_f( void ) {
	if ( sega_job.status != SEGA_JOB_RUNNING ) {
		Com_Printf( "SEGA: no active generation to cancel\n" );
		return;
	}
	sega_job.status = SEGA_JOB_FAILED;
	Q_strncpyz( sega_job.error_msg, "Cancelled by user", sizeof( sega_job.error_msg ) );
	CL_SegaJoinThread();
	Com_Printf( "SEGA: background job stopped (subprocess may still run until exit)\n" );
	if ( sega_job.output_full[0] ) {
		remove( sega_job.output_full );
	}
	sega_job.status = SEGA_JOB_IDLE;
	Com_Printf( "SEGA: generation cancelled\n" );
}

void CL_SegaView_f( void ) {
	const char *filename;

	if ( Cmd_Argc() < 2 ) {
		if ( sega_job.status == SEGA_JOB_COMPLETED && sega_job.output_vfs[0] ) {
			filename = sega_job.output_vfs;
			Com_Printf( "SEGA: using completed job output: %s\n", filename );
		} else {
			Com_Printf( S_COLOR_YELLOW "Usage: sega_view <png_path>\n" );
			Com_Printf( S_COLOR_YELLOW "Example: sega_view screenshots/sega/sega_12345.png\n" );
			return;
		}
	} else {
		filename = Cmd_Argv( 1 );
	}

	CL_SegaReloadTexture( filename );

	if ( sega_job.status == SEGA_JOB_COMPLETED &&
		filename && sega_job.output_vfs[0] && !Q_stricmp( filename, sega_job.output_vfs ) ) {
		CL_SegaJoinThread();
		sega_job.status = SEGA_JOB_IDLE;
		Com_Printf( S_COLOR_GREEN "SEGA: job completed and cleaned up\n" );
	}
}

#endif /* USE_SEGA */
