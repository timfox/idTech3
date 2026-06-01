/*
===========================================================================
Spectral-Energy Guided Attention — external FLUX hi-res generation hook.
See docs/SPEC_ENERGY.md and external/flux_spec_energy/README.md.
===========================================================================
*/

#include "client.h"
#include "cl_spec_energy.h"
#include "cl_pipeline.h"

#ifdef USE_SPEC_ENERGY

#if USE_SDL
#include <SDL2/SDL_thread.h>
#else
typedef struct SDL_Thread SDL_Thread;
#endif

cvar_t *cl_spec_energy_enable;
cvar_t *cl_spec_energy_async;
cvar_t *cl_spec_energy_auto_view;
cvar_t *cl_spec_energy_repo;
cvar_t *cl_spec_energy_python;
cvar_t *cl_spec_energy_conda;
cvar_t *cl_spec_energy_cmd;
cvar_t *cl_spec_energy_width;
cvar_t *cl_spec_energy_height;
cvar_t *cl_spec_energy_steps;
cvar_t *cl_spec_energy_seed;
cvar_t *cl_spec_energy_checkpoint;
cvar_t *cl_spec_energy_multi_gpu;
cvar_t *cl_spec_energy_timeout;

typedef enum {
	SPEC_ENERGY_JOB_IDLE,
	SPEC_ENERGY_JOB_RUNNING,
	SPEC_ENERGY_JOB_COMPLETED,
	SPEC_ENERGY_JOB_FAILED
} spec_energy_job_status_t;

typedef struct {
	spec_energy_job_status_t status;
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
} spec_energy_job_t;

static spec_energy_job_t spec_energy_job;

static const char *CL_SpecEnergyDefaultCmd( void ) {
	return "conda run -n %N --no-capture-output %P \"%E/spec_energy_flux_generate.py\" --repo \"%R\" --prompt \"%I\" --output \"%O\" --checkpoint \"%M\" %A";
}

static qboolean CL_SpecEnergyFileExists( const char *path ) {
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

static void CL_SpecEnergyResolvePath( const char *base, const char *in, char *out, size_t out_size ) {
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

static void CL_SpecEnergyEnsureOutputDir( const char *output_full ) {
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

static qboolean CL_SpecEnergyRunExpanded( const char *tmpl, const cl_pipeline_expand_t *ex, qboolean quiet ) {
	char cmd[8192];

	if ( !CL_PipelineExpandTemplate( cmd, sizeof( cmd ), tmpl, ex ) ) {
		if ( !quiet ) {
			Com_Printf( S_COLOR_RED "SpecEnergy: expanded command too long or bad path characters\n" );
		}
		return qfalse;
	}
	if ( !quiet ) {
		Com_Printf( "SpecEnergy: executing: %s\n", cmd );
	}
	if ( system( cmd ) != 0 ) {
		if ( !quiet ) {
			Com_Printf( S_COLOR_RED "SpecEnergy: shell returned non-zero\n" );
		}
		return qfalse;
	}
	return qtrue;
}

static void CL_SpecEnergyBuildExtraArgs( char *out, size_t out_size ) {
	int width;
	int height;
	int steps;
	int seed;

	if ( !out || out_size == 0 ) {
		return;
	}
	out[0] = '\0';
	width = cl_spec_energy_width ? cl_spec_energy_width->integer : 4096;
	height = cl_spec_energy_height ? cl_spec_energy_height->integer : 4096;
	steps = cl_spec_energy_steps ? cl_spec_energy_steps->integer : 28;
	seed = cl_spec_energy_seed ? cl_spec_energy_seed->integer : 0;
	Com_sprintf( out, out_size, "--height %d --width %d --steps %d --seed %d",
		height, width, steps, seed );
	if ( cl_spec_energy_multi_gpu && cl_spec_energy_multi_gpu->integer ) {
		Q_strcat( out, out_size, " --multi-gpu" );
	}
}

static qboolean CL_SpecEnergyBuildExpandFromJob( cl_pipeline_expand_t *ex ) {
	const char *base;
	const char *py;
	const char *conda;

	if ( !ex ) {
		return qfalse;
	}
	base = Sys_DefaultBasePath();
	py = ( cl_spec_energy_python && cl_spec_energy_python->string[0] ) ? cl_spec_energy_python->string : "python3";
	conda = ( cl_spec_energy_conda && cl_spec_energy_conda->string[0] ) ? cl_spec_energy_conda->string : "spec_energy";
	Com_Memset( ex, 0, sizeof( *ex ) );
	ex->repo = spec_energy_job.repo;
	ex->base = base;
	ex->engine = base;
	ex->py = py;
	ex->conda = conda;
	ex->image = spec_energy_job.prompt;
	ex->output = spec_energy_job.output_full;
	ex->model = spec_energy_job.checkpoint;
	ex->args = spec_energy_job.extra_args;
	return qtrue;
}

static void CL_SpecEnergyJoinThread( void ) {
#if USE_SDL
	if ( spec_energy_job.thread ) {
		int rc = 0;
		SDL_WaitThread( spec_energy_job.thread, &rc );
		spec_energy_job.thread = NULL;
		(void)rc;
	}
#endif
}

static void CL_SpecEnergyReloadTexture( const char *vfs_path ) {
	qhandle_t shader;

	if ( !vfs_path || !vfs_path[0] ) {
		return;
	}
	if ( re.ReloadTexture && re.ReloadTexture( vfs_path ) ) {
		Com_Printf( S_COLOR_GREEN "SpecEnergy: texture reloaded: %s\n", vfs_path );
	}
	shader = re.RegisterShader( vfs_path );
	if ( shader ) {
		Com_Printf( S_COLOR_GREEN "SpecEnergy: shader registered: '%s' (handle %d)\n", vfs_path, shader );
	} else {
		Com_Printf( S_COLOR_YELLOW "SpecEnergy: auto-view failed to register shader for %s\n", vfs_path );
	}
}

static void CL_SpecEnergyFinalizeOnMain( void ) {
	if ( spec_energy_job.status != SPEC_ENERGY_JOB_COMPLETED ) {
		return;
	}
	Com_Printf( S_COLOR_GREEN "SpecEnergy: wrote %s — spec_energy_view %s\n",
		spec_energy_job.output_vfs, spec_energy_job.output_vfs );
	if ( cl_spec_energy_auto_view && cl_spec_energy_auto_view->integer ) {
		CL_SpecEnergyReloadTexture( spec_energy_job.output_vfs );
	}
}

#if USE_SDL
static int CL_SpecEnergyGenerationThread( void *data ) {
	spec_energy_job_t *job = (spec_energy_job_t *)data;
	cl_pipeline_expand_t ex;
	const char *tmpl;

	if ( !job ) {
		return -1;
	}
	if ( !CL_SpecEnergyBuildExpandFromJob( &ex ) ) {
		Q_strncpyz( job->error_msg, "Failed to build Spec-energy command", sizeof( job->error_msg ) );
		job->status = SPEC_ENERGY_JOB_FAILED;
		return -1;
	}
	tmpl = ( cl_spec_energy_cmd && cl_spec_energy_cmd->string[0] ) ? cl_spec_energy_cmd->string : CL_SpecEnergyDefaultCmd();
	if ( !CL_SpecEnergyRunExpanded( tmpl, &ex, qtrue ) ) {
		Q_strncpyz( job->error_msg, "Spec-energy generation subprocess failed", sizeof( job->error_msg ) );
		job->status = SPEC_ENERGY_JOB_FAILED;
		return -1;
	}
	if ( !CL_SpecEnergyFileExists( job->output_full ) ) {
		Q_strncpyz( job->error_msg, "Spec-energy output PNG missing after subprocess", sizeof( job->error_msg ) );
		job->status = SPEC_ENERGY_JOB_FAILED;
		return -1;
	}
	job->status = SPEC_ENERGY_JOB_COMPLETED;
	return 0;
}
#endif

static qboolean CL_SpecEnergyStartJob( const char *prompt ) {
	cl_pipeline_expand_t ex;
	const char *tmpl;
	const char *base;
	const char *repo;
	const char *checkpoint;

	if ( spec_energy_job.status == SPEC_ENERGY_JOB_RUNNING ) {
		Com_Printf( S_COLOR_YELLOW "SpecEnergy: generation already in progress (spec_energy_status / spec_energy_cancel)\n" );
		return qfalse;
	}
	if ( !prompt || !prompt[0] ) {
		Com_Printf( S_COLOR_YELLOW "SpecEnergy: empty prompt\n" );
		return qfalse;
	}
	repo = cl_spec_energy_repo ? cl_spec_energy_repo->string : "";
	if ( !repo || !repo[0] ) {
		Com_Printf( S_COLOR_YELLOW "SpecEnergy: set cl_spec_energy_repo to your upstream checkout (see docs/SPEC_ENERGY.md)\n" );
		return qfalse;
	}
	base = Sys_DefaultBasePath();
	if ( !base ) {
		Com_Printf( S_COLOR_RED "SpecEnergy: no engine base path\n" );
		return qfalse;
	}
	Com_Memset( &spec_energy_job, 0, sizeof( spec_energy_job ) );
	spec_energy_job.start_time = Com_Milliseconds();
	spec_energy_job.timeout_seconds = cl_spec_energy_timeout ? cl_spec_energy_timeout->integer : 7200;
	Q_strncpyz( spec_energy_job.repo, repo, sizeof( spec_energy_job.repo ) );
	Q_strncpyz( spec_energy_job.prompt, prompt, sizeof( spec_energy_job.prompt ) );
	checkpoint = ( cl_spec_energy_checkpoint && cl_spec_energy_checkpoint->string[0] ) ?
		cl_spec_energy_checkpoint->string : "Krea-dev";
	Q_strncpyz( spec_energy_job.checkpoint, checkpoint, sizeof( spec_energy_job.checkpoint ) );
	CL_SpecEnergyBuildExtraArgs( spec_energy_job.extra_args, sizeof( spec_energy_job.extra_args ) );
	Com_sprintf( spec_energy_job.output_vfs, sizeof( spec_energy_job.output_vfs ),
		"screenshots/spec_energy/spec_energy_%d.png", Com_Milliseconds() );
	CL_SpecEnergyResolvePath( base, spec_energy_job.output_vfs, spec_energy_job.output_full, sizeof( spec_energy_job.output_full ) );
	CL_SpecEnergyEnsureOutputDir( spec_energy_job.output_full );

	if ( cl_spec_energy_async && cl_spec_energy_async->integer ) {
#if USE_SDL
		spec_energy_job.status = SPEC_ENERGY_JOB_RUNNING;
		spec_energy_job.thread = SDL_CreateThread( CL_SpecEnergyGenerationThread, "SpecEnergy_Generation", &spec_energy_job );
		if ( !spec_energy_job.thread ) {
			spec_energy_job.status = SPEC_ENERGY_JOB_IDLE;
			Com_Printf( S_COLOR_RED "SpecEnergy: failed to create background thread\n" );
			return qfalse;
		}
		Com_Printf( "SpecEnergy: started background generation -> %s\n", spec_energy_job.output_vfs );
		Com_Printf( "SpecEnergy: Use spec_energy_status / spec_energy_cancel; spec_energy_view when complete\n" );
		return qtrue;
#else
		Com_Printf( S_COLOR_YELLOW "SpecEnergy: async requires SDL threads; running synchronously\n" );
#endif
	}

	spec_energy_job.status = SPEC_ENERGY_JOB_RUNNING;
	if ( !CL_SpecEnergyBuildExpandFromJob( &ex ) ) {
		spec_energy_job.status = SPEC_ENERGY_JOB_IDLE;
		return qfalse;
	}
	tmpl = ( cl_spec_energy_cmd && cl_spec_energy_cmd->string[0] ) ? cl_spec_energy_cmd->string : CL_SpecEnergyDefaultCmd();
	if ( !CL_SpecEnergyRunExpanded( tmpl, &ex, qfalse ) ||
		!CL_SpecEnergyFileExists( spec_energy_job.output_full ) ) {
		Q_strncpyz( spec_energy_job.error_msg, "Spec-energy generation failed", sizeof( spec_energy_job.error_msg ) );
		spec_energy_job.status = SPEC_ENERGY_JOB_FAILED;
		Com_Printf( S_COLOR_RED "SpecEnergy: generation failed\n" );
		return qfalse;
	}
	spec_energy_job.status = SPEC_ENERGY_JOB_COMPLETED;
	CL_SpecEnergyFinalizeOnMain();
	spec_energy_job.notified = qtrue;
	return qtrue;
}

void CL_SpecEnergyFrame( void ) {
	if ( spec_energy_job.status == SPEC_ENERGY_JOB_RUNNING && spec_energy_job.timeout_seconds > 0 ) {
		int runtime = ( Com_Milliseconds() - spec_energy_job.start_time ) / 1000;
		if ( runtime > spec_energy_job.timeout_seconds ) {
			Com_Printf( S_COLOR_YELLOW "SpecEnergy: generation exceeded %d s (still running); spec_energy_cancel to stop\n",
				spec_energy_job.timeout_seconds );
			spec_energy_job.timeout_seconds = 0;
		}
		return;
	}
	if ( spec_energy_job.status != SPEC_ENERGY_JOB_COMPLETED && spec_energy_job.status != SPEC_ENERGY_JOB_FAILED ) {
		return;
	}
	if ( spec_energy_job.notified ) {
		return;
	}
	CL_SpecEnergyJoinThread();
	spec_energy_job.notified = qtrue;
	if ( spec_energy_job.status == SPEC_ENERGY_JOB_COMPLETED ) {
		CL_SpecEnergyFinalizeOnMain();
	} else {
		Com_Printf( S_COLOR_RED "SpecEnergy: generation failed: %s\n", spec_energy_job.error_msg );
	}
}

void CL_SpecEnergyInit( void ) {
	cl_spec_energy_enable = Cvar_Get( "cl_spec_energy_enable", "0", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_spec_energy_enable, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_spec_energy_enable,
		"Enable Spec-energy hi-res FLUX generation (training-free attention rescaling). See docs/SPEC_ENERGY.md." );

	cl_spec_energy_async = Cvar_Get( "cl_spec_energy_async", "1", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_spec_energy_async, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_spec_energy_async,
		"Spec-energy mode: 0=blocking console, 1=background SDL thread (recommended)." );

	cl_spec_energy_auto_view = Cvar_Get( "cl_spec_energy_auto_view", "0", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_spec_energy_auto_view, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_spec_energy_auto_view,
		"When 1, reload/register the output PNG when generation completes." );

	cl_spec_energy_timeout = Cvar_Get( "cl_spec_energy_timeout", "7200", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_spec_energy_timeout, "60", "86400", CV_INTEGER );
	Cvar_SetDescription( cl_spec_energy_timeout, "Warn after this many seconds if a background Spec-energy job is still running." );

	cl_spec_energy_repo = Cvar_Get( "cl_spec_energy_repo", "", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_spec_energy_repo,
		"Absolute path to upstream Spectral-Energy git checkout (%%R in cl_spec_energy_cmd / spec_energy_generate)." );

	cl_spec_energy_python = Cvar_Get( "cl_spec_energy_python", "python3", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_spec_energy_python, "Python interpreter (%%P) when not using conda run." );

	cl_spec_energy_conda = Cvar_Get( "cl_spec_energy_conda", "spec_energy", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_spec_energy_conda, "Conda environment name (%%N) for default spec_energy_generate command." );

	cl_spec_energy_cmd = Cvar_Get( "cl_spec_energy_cmd", "", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_spec_energy_cmd,
		"Optional shell template for spec_energy_generate. Tokens: %%R repo, %%B base, %%E release dir, %%P python, %%N conda, %%I prompt, %%O output png, %%M checkpoint, %%A extra args." );

	cl_spec_energy_width = Cvar_Get( "cl_spec_energy_width", "4096", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_spec_energy_width, "512", "8192", CV_INTEGER );
	Cvar_SetDescription( cl_spec_energy_width, "Output width in pixels (passed via %%A)." );

	cl_spec_energy_height = Cvar_Get( "cl_spec_energy_height", "4096", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_spec_energy_height, "512", "8192", CV_INTEGER );
	Cvar_SetDescription( cl_spec_energy_height, "Output height in pixels (passed via %%A)." );

	cl_spec_energy_steps = Cvar_Get( "cl_spec_energy_steps", "28", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_spec_energy_steps, "1", "100", CV_INTEGER );
	Cvar_SetDescription( cl_spec_energy_steps, "Denoising steps for spec-energy FLUX (passed via %%A)." );

	cl_spec_energy_seed = Cvar_Get( "cl_spec_energy_seed", "0", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_spec_energy_seed, "0", "2147483647", CV_INTEGER );
	Cvar_SetDescription( cl_spec_energy_seed, "Random seed for reproducible generation (passed via %%A)." );

	cl_spec_energy_checkpoint = Cvar_Get( "cl_spec_energy_checkpoint", "Krea-dev", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_spec_energy_checkpoint,
		"FLUX checkpoint shorthand (dev, Krea-dev) or full HF repo id (%%M)." );

	cl_spec_energy_multi_gpu = Cvar_Get( "cl_spec_energy_multi_gpu", "0", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_spec_energy_multi_gpu, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_spec_energy_multi_gpu,
		"When 1, pass --multi-gpu to the wrapper (requires 2+ visible CUDA devices)." );

	Cmd_AddCommand( "spec_energy_generate", CL_SpecEnergyGenerate_f );
	Cmd_AddCommand( "spec_energy_status", CL_SpecEnergyStatus_f );
	Cmd_AddCommand( "spec_energy_cancel", CL_SpecEnergyCancel_f );
	Cmd_AddCommand( "spec_energy_view", CL_SpecEnergyView_f );

	Com_Memset( &spec_energy_job, 0, sizeof( spec_energy_job ) );

	if ( cl_spec_energy_enable && cl_spec_energy_enable->integer ) {
		Com_Printf( "Spec-energy hi-res FLUX: enabled (repo: %s, %dx%d, async: %s)\n",
			( cl_spec_energy_repo && cl_spec_energy_repo->string[0] ) ? cl_spec_energy_repo->string : "unset",
			cl_spec_energy_width ? cl_spec_energy_width->integer : 4096,
			cl_spec_energy_height ? cl_spec_energy_height->integer : 4096,
			( cl_spec_energy_async && cl_spec_energy_async->integer ) ? "on" : "off" );
	} else {
		Com_Printf( "Spec-energy hi-res FLUX: disabled (cl_spec_energy_enable 0; docs/SPEC_ENERGY.md)\n" );
	}
}

void CL_SpecEnergyShutdown( void ) {
#if USE_SDL
	if ( spec_energy_job.thread ) {
		SDL_WaitThread( spec_energy_job.thread, NULL );
		spec_energy_job.thread = NULL;
	}
#endif
	spec_energy_job.status = SPEC_ENERGY_JOB_IDLE;
	Cmd_RemoveCommand( "spec_energy_generate" );
	Cmd_RemoveCommand( "spec_energy_status" );
	Cmd_RemoveCommand( "spec_energy_cancel" );
	Cmd_RemoveCommand( "spec_energy_view" );
}

void CL_SpecEnergyGenerate_f( void ) {
	const char *prompt;

	if ( !cl_spec_energy_enable || !cl_spec_energy_enable->integer ) {
		Com_Printf( S_COLOR_YELLOW "spec_energy_generate: set cl_spec_energy_enable 1 (see docs/SPEC_ENERGY.md)\n" );
		return;
	}
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( S_COLOR_YELLOW "Usage: spec_energy_generate <prompt>\n" );
		Com_Printf( "  Example: spec_energy_generate \"a misty mountain at dawn\"\n" );
		return;
	}
	prompt = Cmd_ArgsFrom( 1 );
	CL_SpecEnergyStartJob( prompt );
}

void CL_SpecEnergyStatus_f( void ) {
	switch ( spec_energy_job.status ) {
	case SPEC_ENERGY_JOB_IDLE:
		Com_Printf( "SpecEnergy: no generation in progress\n" );
		break;
	case SPEC_ENERGY_JOB_RUNNING: {
		int runtime = ( Com_Milliseconds() - spec_energy_job.start_time ) / 1000;
		Com_Printf( "SpecEnergy: generation in progress (%d s)\n", runtime );
		Com_Printf( "SpecEnergy: prompt: %s\n", spec_energy_job.prompt );
		Com_Printf( "SpecEnergy: output: %s\n", spec_energy_job.output_vfs );
		break;
	}
	case SPEC_ENERGY_JOB_COMPLETED:
		Com_Printf( S_COLOR_GREEN "SpecEnergy: complete: %s\n", spec_energy_job.output_vfs );
		Com_Printf( "SpecEnergy: spec_energy_view to hot-reload texture\n" );
		break;
	case SPEC_ENERGY_JOB_FAILED:
		Com_Printf( S_COLOR_RED "SpecEnergy: failed: %s\n", spec_energy_job.error_msg );
		break;
	}
}

void CL_SpecEnergyCancel_f( void ) {
	if ( spec_energy_job.status != SPEC_ENERGY_JOB_RUNNING ) {
		Com_Printf( "SpecEnergy: no active generation to cancel\n" );
		return;
	}
	spec_energy_job.status = SPEC_ENERGY_JOB_FAILED;
	Q_strncpyz( spec_energy_job.error_msg, "Cancelled by user", sizeof( spec_energy_job.error_msg ) );
	CL_SpecEnergyJoinThread();
	Com_Printf( "SpecEnergy: background job stopped (subprocess may still run until exit)\n" );
	if ( spec_energy_job.output_full[0] ) {
		remove( spec_energy_job.output_full );
	}
	spec_energy_job.status = SPEC_ENERGY_JOB_IDLE;
	Com_Printf( "SpecEnergy: generation cancelled\n" );
}

void CL_SpecEnergyView_f( void ) {
	const char *filename;

	if ( Cmd_Argc() < 2 ) {
		if ( spec_energy_job.status == SPEC_ENERGY_JOB_COMPLETED && spec_energy_job.output_vfs[0] ) {
			filename = spec_energy_job.output_vfs;
			Com_Printf( "SpecEnergy: using completed job output: %s\n", filename );
		} else {
			Com_Printf( S_COLOR_YELLOW "Usage: spec_energy_view <png_path>\n" );
			Com_Printf( S_COLOR_YELLOW "Example: spec_energy_view screenshots/spec_energy/spec_energy_12345.png\n" );
			return;
		}
	} else {
		filename = Cmd_Argv( 1 );
	}

	CL_SpecEnergyReloadTexture( filename );

	if ( spec_energy_job.status == SPEC_ENERGY_JOB_COMPLETED &&
		filename && spec_energy_job.output_vfs[0] && !Q_stricmp( filename, spec_energy_job.output_vfs ) ) {
		CL_SpecEnergyJoinThread();
		spec_energy_job.status = SPEC_ENERGY_JOB_IDLE;
		Com_Printf( S_COLOR_GREEN "SpecEnergy: job completed and cleaned up\n" );
	}
}

#endif /* USE_SPEC_ENERGY */
