/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "client.h"
#include "cl_trellis.h"
#include "cl_ml_worker.h"
#include "cl_pipeline.h"
#if defined( USE_FLUX )
#include "cl_flux.h"
#endif

#include <stdio.h>
#include <string.h>

#if ( defined( USE_FLUX ) || defined( USE_TRELLIS ) ) && USE_SDL
#include <SDL3/SDL.h>
#elif defined( USE_FLUX ) || defined( USE_TRELLIS )
typedef struct SDL_Thread SDL_Thread;
#endif

#ifdef USE_TRELLIS

cvar_t	*cl_trellis_enable;
cvar_t	*cl_trellis_async;
cvar_t	*cl_trellis_auto_import;
cvar_t	*cl_trellis_chain;
cvar_t	*cl_trellis_repo;
cvar_t	*cl_trellis_python;
cvar_t	*cl_trellis_conda;
cvar_t	*cl_trellis_cmd;
cvar_t	*cl_trellis_hf_model;
cvar_t	*cl_trellis_decimation;
cvar_t	*cl_trellis_texture_size;
cvar_t	*cl_trellis_timeout;

typedef enum {
	TRELLIS_JOB_IDLE,
	TRELLIS_JOB_RUNNING,
	TRELLIS_JOB_COMPLETED,
	TRELLIS_JOB_FAILED
} trellis_job_status_t;

typedef struct {
	trellis_job_status_t status;
	qboolean notified;
	int start_time;
	int timeout_seconds;
	char repo[MAX_OSPATH];
	char image_full[MAX_OSPATH];
	char image_vfs[MAX_QPATH];
	char output_full[MAX_OSPATH];
	char output_vfs[MAX_QPATH];
	char hf_model[256];
	char dec_str[32];
	char tex_str[32];
	char error_msg[1024];
	qhandle_t model_handle;
} trellis_job_t;

static trellis_job_t trellis_job;
static clMlTask_t s_trellisTask;

static const char *CL_TrellisDefaultCmd( void ) {
	return "conda run -n %N --no-capture-output %P \"%E/trellis_image_to_glb.py\" --repo \"%R\" --image \"%I\" --output \"%O\" --model \"%M\" --decimation %D --texture-size %T %A";
}

static qboolean CL_TrellisRunExpanded( const char *tmpl, const cl_pipeline_expand_t *ex, qboolean quiet ) {
	return CL_PipelineRunTemplate( tmpl, ex, quiet, "TRELLIS" );
}

static qboolean CL_TrellisBuildExpandFromJob( cl_pipeline_expand_t *ex ) {
	const char *base;
	const char *py;
	const char *conda;

	if ( !ex ) {
		return qfalse;
	}
	base = Sys_DefaultBasePath();
	py = ( cl_trellis_python && cl_trellis_python->string[0] ) ? cl_trellis_python->string : "python3";
	conda = ( cl_trellis_conda && cl_trellis_conda->string[0] ) ? cl_trellis_conda->string : "trellis2";
	Com_Memset( ex, 0, sizeof( *ex ) );
	ex->repo = trellis_job.repo;
	ex->base = base;
	ex->engine = base;
	ex->py = py;
	ex->conda = conda;
	ex->image = trellis_job.image_full;
	ex->output = trellis_job.output_full;
	ex->model = trellis_job.hf_model;
	ex->decimation = trellis_job.dec_str;
	ex->texture_size = trellis_job.tex_str;
	ex->args = "";
	return qtrue;
}

static qhandle_t CL_TrellisImportVfs( const char *vfs_path ) {
	if ( !vfs_path || !vfs_path[0] || !re.RegisterModel ) {
		return 0;
	}
	return re.RegisterModel( vfs_path );
}

static void CL_TrellisSetJobResult( qboolean success ) {
	if ( success ) {
		trellis_job.status = TRELLIS_JOB_COMPLETED;
	} else {
		trellis_job.status = TRELLIS_JOB_FAILED;
	}
}

/*
CL_TrellisFinalizeOnMain
Must run on the main thread (RegisterModel / renderer).
*/
static void CL_TrellisFinalizeOnMain( void ) {
	if ( trellis_job.status != TRELLIS_JOB_COMPLETED ) {
		return;
	}
	if ( cl_trellis_auto_import && cl_trellis_auto_import->integer ) {
		trellis_job.model_handle = CL_TrellisImportVfs( trellis_job.output_vfs );
		if ( trellis_job.model_handle ) {
			Com_Printf( S_COLOR_GREEN "TRELLIS: auto-imported '%s' (handle %d)\n",
				trellis_job.output_vfs, trellis_job.model_handle );
		} else {
			Com_Printf( S_COLOR_YELLOW "TRELLIS: GLB written but auto-import failed; try trellis_view %s\n",
				trellis_job.output_vfs );
		}
	} else {
		Com_Printf( S_COLOR_GREEN "TRELLIS: wrote %s — trellis_view %s\n",
			trellis_job.output_vfs, trellis_job.output_vfs );
	}
}

static void CL_Trellis_DeferFinalize( void *data )
{
	trellis_job_t *job = (trellis_job_t *)data;

	if ( !job ) {
		CL_MlWorker_Release( "trellis" );
		return;
	}

	job->notified = qtrue;
	if ( job->status == TRELLIS_JOB_COMPLETED ) {
		CL_TrellisFinalizeOnMain();
	} else if ( job->status == TRELLIS_JOB_FAILED ) {
		Com_Printf( S_COLOR_RED "TRELLIS: generation failed: %s\n", job->error_msg );
	}

	CL_MlWorker_Release( "trellis" );
}

static void CL_Trellis_Worker( void *data )
{
	trellis_job_t *job = (trellis_job_t *)data;
	cl_pipeline_expand_t ex;
	const char *tmpl;

	if ( !job ) {
		return;
	}
	if ( !CL_TrellisBuildExpandFromJob( &ex ) ) {
		Q_strncpyz( job->error_msg, "Failed to build TRELLIS command", sizeof( job->error_msg ) );
		job->status = TRELLIS_JOB_FAILED;
		return;
	}
	tmpl = ( cl_trellis_cmd && cl_trellis_cmd->string[0] ) ? cl_trellis_cmd->string : CL_TrellisDefaultCmd();
	if ( !CL_TrellisRunExpanded( tmpl, &ex, qtrue ) ) {
		Q_strncpyz( job->error_msg, "TRELLIS generation subprocess failed", sizeof( job->error_msg ) );
		job->status = TRELLIS_JOB_FAILED;
		return;
	}
	if ( !CL_PipelineFileExists( job->output_full ) ) {
		Q_strncpyz( job->error_msg, "TRELLIS output GLB missing after subprocess", sizeof( job->error_msg ) );
		job->status = TRELLIS_JOB_FAILED;
		return;
	}
	CL_TrellisSetJobResult( qtrue );
}

qboolean CL_Trellis_StartJob( const char *image_rel, const char *output_rel_optional ) {
	cl_pipeline_expand_t ex;
	const char *tmpl;
	const char *base;
	const char *repo;
	const char *hf_model;

	if ( trellis_job.status == TRELLIS_JOB_RUNNING ) {
		Com_Printf( S_COLOR_YELLOW "TRELLIS: generation already in progress (trellis_status / trellis_cancel)\n" );
		return qfalse;
	}
	repo = cl_trellis_repo ? cl_trellis_repo->string : "";
	if ( !repo || !repo[0] ) {
		Com_Printf( S_COLOR_YELLOW "TRELLIS: set cl_trellis_repo to your TRELLIS.2 checkout\n" );
		return qfalse;
	}
	base = Sys_DefaultBasePath();
	if ( !base ) {
		Com_Printf( S_COLOR_RED "TRELLIS: no engine base path\n" );
		return qfalse;
	}
	Com_Memset( &trellis_job, 0, sizeof( trellis_job ) );
	trellis_job.start_time = Com_Milliseconds();
	trellis_job.timeout_seconds = cl_trellis_timeout ? cl_trellis_timeout->integer : 3600;
	Q_strncpyz( trellis_job.repo, repo, sizeof( trellis_job.repo ) );
	Q_strncpyz( trellis_job.image_vfs, image_rel, sizeof( trellis_job.image_vfs ) );
	CL_PipelineResolvePath( base, image_rel, trellis_job.image_full, sizeof( trellis_job.image_full ) );
	if ( !CL_PipelineFileExists( trellis_job.image_full ) ) {
		Com_Printf( S_COLOR_RED "TRELLIS: input image not found: %s\n", trellis_job.image_full );
		return qfalse;
	}
	if ( output_rel_optional && output_rel_optional[0] ) {
		Q_strncpyz( trellis_job.output_vfs, output_rel_optional, sizeof( trellis_job.output_vfs ) );
	} else {
		Com_sprintf( trellis_job.output_vfs, sizeof( trellis_job.output_vfs ),
			"models/trellis/trellis_%d.glb", Com_Milliseconds() );
	}
	if ( !strstr( trellis_job.output_vfs, ".glb" ) && !strstr( trellis_job.output_vfs, ".gltf" ) ) {
		Q_strcat( trellis_job.output_vfs, sizeof( trellis_job.output_vfs ), ".glb" );
	}
	CL_PipelineResolvePath( base, trellis_job.output_vfs, trellis_job.output_full, sizeof( trellis_job.output_full ) );
	CL_PipelineEnsureOutputDir( trellis_job.output_full );
	hf_model = ( cl_trellis_hf_model && cl_trellis_hf_model->string[0] ) ?
		cl_trellis_hf_model->string : "microsoft/TRELLIS.2-4B";
	Q_strncpyz( trellis_job.hf_model, hf_model, sizeof( trellis_job.hf_model ) );
	Com_sprintf( trellis_job.dec_str, sizeof( trellis_job.dec_str ), "%d",
		cl_trellis_decimation ? cl_trellis_decimation->integer : 500000 );
	Com_sprintf( trellis_job.tex_str, sizeof( trellis_job.tex_str ), "%d",
		cl_trellis_texture_size ? cl_trellis_texture_size->integer : 2048 );

	if ( cl_trellis_async && cl_trellis_async->integer ) {
		if ( CL_MlWorker_IsBusy() ) {
			Com_Printf( S_COLOR_YELLOW "TRELLIS: ML worker busy (%s)\n", CL_MlWorker_Owner() );
			return qfalse;
		}

		trellis_job.status = TRELLIS_JOB_RUNNING;
		trellis_job.notified = qfalse;

		CL_MlWorker_InitTask( &s_trellisTask, "trellis", CL_Trellis_Worker, CL_Trellis_DeferFinalize, &trellis_job );

		if ( !CL_MlWorker_Submit( &s_trellisTask ) ) {
			trellis_job.status = TRELLIS_JOB_IDLE;
			Com_Printf( S_COLOR_RED "TRELLIS: failed to start generation worker\n" );
			return qfalse;
		}

		Com_Printf( "TRELLIS: started background generation from %s -> %s\n",
			trellis_job.image_vfs, trellis_job.output_vfs );
		Com_Printf( "TRELLIS: Use trellis_status / trellis_cancel; model auto-imports when cl_trellis_auto_import 1\n" );
		return qtrue;
	}

	trellis_job.status = TRELLIS_JOB_RUNNING;
	if ( !CL_TrellisBuildExpandFromJob( &ex ) ) {
		trellis_job.status = TRELLIS_JOB_IDLE;
		return qfalse;
	}
	tmpl = ( cl_trellis_cmd && cl_trellis_cmd->string[0] ) ? cl_trellis_cmd->string : CL_TrellisDefaultCmd();
	if ( !CL_TrellisRunExpanded( tmpl, &ex, qfalse ) ||
		!CL_PipelineFileExists( trellis_job.output_full ) ) {
		Q_strncpyz( trellis_job.error_msg, "TRELLIS generation failed", sizeof( trellis_job.error_msg ) );
		trellis_job.status = TRELLIS_JOB_FAILED;
		Com_Printf( S_COLOR_RED "TRELLIS: generation failed\n" );
		return qfalse;
	}
	CL_TrellisSetJobResult( qtrue );
	CL_TrellisFinalizeOnMain();
	trellis_job.notified = qtrue;
	return qtrue;
}

void CL_Trellis_Frame( void ) {
	if ( trellis_job.status == TRELLIS_JOB_RUNNING ) {
		if ( trellis_job.timeout_seconds > 0 ) {
			int runtime = ( Com_Milliseconds() - trellis_job.start_time ) / 1000;
			if ( runtime > trellis_job.timeout_seconds ) {
				Com_Printf( S_COLOR_YELLOW "TRELLIS: generation exceeded %d s (still running); trellis_cancel to stop\n",
					trellis_job.timeout_seconds );
				trellis_job.timeout_seconds = 0;
			}
		}
		return;
	}
	if ( trellis_job.status != TRELLIS_JOB_COMPLETED && trellis_job.status != TRELLIS_JOB_FAILED ) {
		return;
	}
	if ( trellis_job.notified ) {
		return;
	}
	/* Sync path only — async completion runs in CL_Trellis_DeferFinalize */
	trellis_job.notified = qtrue;
	if ( trellis_job.status == TRELLIS_JOB_COMPLETED ) {
		CL_TrellisFinalizeOnMain();
	} else {
		Com_Printf( S_COLOR_RED "TRELLIS: generation failed: %s\n", trellis_job.error_msg );
	}
}


/*
==================
CL_TrellisPipeline_f

Run a user-configured shell command for the TRELLIS.2 Python/CUDA stack.
==================
*/
static void CL_TrellisPipeline_f( void ) {
	const char *repo;
	const char *base;
	const char *engine;
	const char *py;
	const char *conda;
	const char *tmpl;
	const char *args;
	cl_pipeline_expand_t ex;

	if ( !cl_trellis_enable || !cl_trellis_enable->integer ) {
		Com_Printf( S_COLOR_YELLOW "trellis_pipeline: set cl_trellis_enable 1 (see docs/TRELLIS.md)\n" );
		return;
	}
	repo = cl_trellis_repo ? cl_trellis_repo->string : "";
	if ( !repo || !repo[0] ) {
		Com_Printf( S_COLOR_YELLOW "trellis_pipeline: set cl_trellis_repo to your TRELLIS.2 checkout\n" );
		return;
	}
	base = Sys_DefaultBasePath();
	engine = base;
	if ( !base ) {
		Com_Printf( S_COLOR_RED "trellis_pipeline: no engine base path\n" );
		return;
	}
	py = ( cl_trellis_python && cl_trellis_python->string[0] ) ? cl_trellis_python->string : "python3";
	conda = ( cl_trellis_conda && cl_trellis_conda->string[0] ) ? cl_trellis_conda->string : "trellis2";
	tmpl = ( cl_trellis_cmd && cl_trellis_cmd->string[0] ) ? cl_trellis_cmd->string : CL_TrellisDefaultCmd();
	args = ( Cmd_Argc() >= 2 ) ? Cmd_ArgsFrom( 1 ) : "";
	Com_Memset( &ex, 0, sizeof( ex ) );
	ex.repo = repo;
	ex.base = base;
	ex.engine = engine;
	ex.py = py;
	ex.conda = conda;
	ex.args = args;
	if ( !CL_TrellisRunExpanded( tmpl, &ex, qfalse ) ) {
		return;
	}
	Com_Printf( S_COLOR_GREEN "trellis_pipeline: finished\n" );
}

/*
==================
CL_TrellisGenerate_f
==================
*/
static void CL_TrellisGenerate_f( void ) {
	const char *output_opt;

	if ( !cl_trellis_enable || !cl_trellis_enable->integer ) {
		Com_Printf( S_COLOR_YELLOW "trellis_generate: set cl_trellis_enable 1 (see docs/TRELLIS.md)\n" );
		return;
	}
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( S_COLOR_YELLOW "Usage: trellis_generate <image_path> [output.glb]\n" );
		Com_Printf( "  Example: trellis_generate screenshots/shot0000.tga\n" );
		return;
	}
	output_opt = ( Cmd_Argc() >= 3 ) ? Cmd_Argv( 2 ) : NULL;
	CL_Trellis_StartJob( Cmd_Argv( 1 ), output_opt );
}

/*
==================
CL_TrellisStatus_f / CL_TrellisCancel_f
==================
*/
static void CL_TrellisStatus_f( void ) {
	switch ( trellis_job.status ) {
	case TRELLIS_JOB_IDLE:
		Com_Printf( "TRELLIS: no generation in progress\n" );
		break;
	case TRELLIS_JOB_RUNNING: {
		int runtime = ( Com_Milliseconds() - trellis_job.start_time ) / 1000;
		Com_Printf( "TRELLIS: generation in progress (%d s)\n", runtime );
		Com_Printf( "TRELLIS: image: %s -> %s\n", trellis_job.image_vfs, trellis_job.output_vfs );
		break;
	}
	case TRELLIS_JOB_COMPLETED:
		Com_Printf( S_COLOR_GREEN "TRELLIS: complete: %s (handle %d)\n",
			trellis_job.output_vfs, trellis_job.model_handle );
		Com_Printf( "TRELLIS: trellis_view to re-register; trellis_show for spawn hint\n" );
		break;
	case TRELLIS_JOB_FAILED:
		Com_Printf( S_COLOR_RED "TRELLIS: failed: %s\n", trellis_job.error_msg );
		break;
	}
}

static void CL_TrellisCancel_f( void ) {
	if ( trellis_job.status != TRELLIS_JOB_RUNNING ) {
		Com_Printf( "TRELLIS: no active generation to cancel\n" );
		return;
	}
	trellis_job.status = TRELLIS_JOB_FAILED;
	Q_strncpyz( trellis_job.error_msg, "Cancelled by user", sizeof( trellis_job.error_msg ) );
	CL_MlWorker_Cancel( &s_trellisTask );
	Com_Printf( "TRELLIS: background job stopped (subprocess may still run until exit)\n" );
	if ( trellis_job.output_full[0] ) {
		remove( trellis_job.output_full );
	}
	trellis_job.status = TRELLIS_JOB_IDLE;
	Com_Printf( "TRELLIS: generation cancelled\n" );
}

/*
==================
CL_TrellisShow_f / CL_TrellisView_f — mirror flux_show / flux_view for models
==================
*/
static void CL_TrellisShow_f( void ) {
	const char *name;

	if ( Cmd_Argc() < 2 ) {
		if ( trellis_job.status == TRELLIS_JOB_COMPLETED && trellis_job.output_vfs[0] ) {
			name = trellis_job.output_vfs;
		} else {
			Com_Printf( S_COLOR_YELLOW "Usage: trellis_show <models/.../asset.glb>\n" );
			return;
		}
	} else {
		name = Cmd_Argv( 1 );
	}
	if ( CL_TrellisImportVfs( name ) ) {
		Com_Printf( S_COLOR_GREEN "TRELLIS: model ready at '%s'\n", name );
		Com_Printf( "  Use in maps: misc_model \"%s\" (or your game's model entity)\n", name );
	} else {
		Com_Printf( S_COLOR_RED "TRELLIS: failed to register '%s'\n", name );
	}
}

static void CL_TrellisView_f( void ) {
	const char *name;
	qhandle_t h;

	if ( Cmd_Argc() < 2 ) {
		if ( trellis_job.status == TRELLIS_JOB_COMPLETED && trellis_job.output_vfs[0] ) {
			name = trellis_job.output_vfs;
			Com_Printf( "TRELLIS: using last job output: %s\n", name );
		} else {
			Com_Printf( S_COLOR_YELLOW "Usage: trellis_view <models/.../asset.glb>\n" );
			return;
		}
	} else {
		name = Cmd_Argv( 1 );
	}
	h = CL_TrellisImportVfs( name );
	if ( !h ) {
		Com_Printf( S_COLOR_RED "TRELLIS: failed to register '%s'\n", name );
		return;
	}
	trellis_job.model_handle = h;
	Com_Printf( S_COLOR_GREEN "TRELLIS: registered '%s' (handle %d)\n", name, h );
	if ( trellis_job.status == TRELLIS_JOB_COMPLETED || trellis_job.status == TRELLIS_JOB_FAILED ) {
		trellis_job.status = TRELLIS_JOB_IDLE;
		trellis_job.notified = qfalse;
	}
}

static void CL_TrellisImport_f( void ) {
	CL_TrellisShow_f();
}

#if defined( USE_FLUX )
/*
==================
CL_TrellisFromPrompt_f — FLUX image then TRELLIS mesh (runtime text-to-3D)
==================
*/
static void CL_TrellisFromPrompt_f( void ) {
	const char *prompt;
	qboolean flux_was_async;

	if ( !cl_trellis_enable || !cl_trellis_enable->integer ) {
		Com_Printf( S_COLOR_YELLOW "trellis_from_prompt: set cl_trellis_enable 1\n" );
		return;
	}
	if ( !cl_flux_enable || !cl_flux_enable->integer ) {
		Com_Printf( S_COLOR_YELLOW "trellis_from_prompt: set cl_flux_enable 1\n" );
		return;
	}
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( S_COLOR_YELLOW "Usage: trellis_from_prompt <text prompt>\n" );
		return;
	}
	if ( CL_Flux_IsRunning() || trellis_job.status == TRELLIS_JOB_RUNNING ) {
		Com_Printf( S_COLOR_YELLOW "trellis_from_prompt: wait for current FLUX/TRELLIS job or cancel\n" );
		return;
	}
	prompt = Cmd_ArgsFrom( 1 );
	flux_was_async = cl_flux_async && cl_flux_async->integer;
	if ( !flux_was_async ) {
		Cvar_Set( "cl_flux_async", "1" );
	}
	Cvar_Set( "cl_trellis_chain", "1" );
	CL_Flux_ArmTrellisChain();
	Cbuf_AddText( "flux_generate \"" );
	Cbuf_AddText( prompt );
	Cbuf_AddText( "\"\n" );
	Com_Printf( "TRELLIS: FLUX started; will chain TRELLIS when image completes (cl_trellis_chain 1)\n" );
	if ( !flux_was_async ) {
		Com_Printf( "TRELLIS: enabled cl_flux_async 1 for this workflow\n" );
	}
}
#endif

void CL_Trellis_Init( void )
{
	cl_trellis_enable = Cvar_Get( "cl_trellis_enable", "0", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_trellis_enable, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_trellis_enable,
		"Enable Microsoft TRELLIS.2 image-to-3D runtime generation (mirrors FLUX workflow). See docs/TRELLIS.md." );
	cl_trellis_async = Cvar_Get( "cl_trellis_async", "1", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_trellis_async, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_trellis_async,
		"TRELLIS mode: 0=blocking console, 1=background thread (recommended for runtime use)." );
	cl_trellis_auto_import = Cvar_Get( "cl_trellis_auto_import", "1", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_trellis_auto_import, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_trellis_auto_import,
		"When 1, register the output .glb via RegisterModel when generation completes." );
#if defined( USE_FLUX )
	cl_trellis_chain = Cvar_Get( "cl_trellis_chain", "0", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_trellis_chain, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_trellis_chain,
		"When 1, trellis_from_prompt (or manual FLUX then chain) runs TRELLIS on FLUX output automatically." );
#endif
	cl_trellis_timeout = Cvar_Get( "cl_trellis_timeout", "3600", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_trellis_timeout, "60", "86400", CV_INTEGER );
	Cvar_SetDescription( cl_trellis_timeout, "Warn after this many seconds if a background TRELLIS job is still running." );
	cl_trellis_repo = Cvar_Get( "cl_trellis_repo", "", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_trellis_repo,
		"Absolute path to a TRELLIS.2 git checkout (%R in cl_trellis_cmd / trellis_generate)." );
	cl_trellis_python = Cvar_Get( "cl_trellis_python", "python3", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_trellis_python, "Python interpreter (%P) when not using conda run." );
	cl_trellis_conda = Cvar_Get( "cl_trellis_conda", "trellis2", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_trellis_conda, "Conda environment name (%N) for default trellis_generate command." );
	cl_trellis_cmd = Cvar_Get( "cl_trellis_cmd", "", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_trellis_cmd,
		"Shell template for trellis_generate / trellis_pipeline. Tokens: %R repo, %B base, %E release, %P python, %N conda, %I image, %O output, %M HF model, %D decimation, %T texture, %A extra args." );
	cl_trellis_hf_model = Cvar_Get( "cl_trellis_hf_model", "microsoft/TRELLIS.2-4B", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_trellis_hf_model, "Hugging Face model id passed to the TRELLIS.2 wrapper (%M)." );
	cl_trellis_decimation = Cvar_Get( "cl_trellis_decimation", "500000", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_trellis_decimation, "1000", "10000000", CV_INTEGER );
	Cvar_SetDescription( cl_trellis_decimation, "GLB decimation target for o_voxel export (%D)." );
	cl_trellis_texture_size = Cvar_Get( "cl_trellis_texture_size", "2048", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_trellis_texture_size, "256", "8192", CV_INTEGER );
	Cvar_SetDescription( cl_trellis_texture_size, "GLB texture atlas size for export (%T)." );

	Cmd_AddCommand( "trellis_generate", CL_TrellisGenerate_f );
	Cmd_AddCommand( "trellis_pipeline", CL_TrellisPipeline_f );
	Cmd_AddCommand( "trellis_import", CL_TrellisImport_f );
	Cmd_AddCommand( "trellis_status", CL_TrellisStatus_f );
	Cmd_AddCommand( "trellis_cancel", CL_TrellisCancel_f );
	Cmd_AddCommand( "trellis_show", CL_TrellisShow_f );
	Cmd_AddCommand( "trellis_view", CL_TrellisView_f );
#if defined( USE_FLUX )
	Cmd_AddCommand( "trellis_from_prompt", CL_TrellisFromPrompt_f );
#endif

	Com_Memset( &trellis_job, 0, sizeof( trellis_job ) );

	if ( cl_trellis_enable && cl_trellis_enable->integer ) {
		Com_Printf( "TRELLIS.2 image-to-3D: enabled (repo: %s, model: %s, async: %s, auto_import: %s)\n",
			( cl_trellis_repo && cl_trellis_repo->string[0] ) ? cl_trellis_repo->string : "unset",
			cl_trellis_hf_model ? cl_trellis_hf_model->string : "microsoft/TRELLIS.2-4B",
			( cl_trellis_async && cl_trellis_async->integer ) ? "on" : "off",
			( cl_trellis_auto_import && cl_trellis_auto_import->integer ) ? "on" : "off" );
	} else {
		Com_Printf( "TRELLIS.2 image-to-3D: disabled (cl_trellis_enable 0; docs/TRELLIS.md)\n" );
	}
}

void CL_Trellis_Shutdown( void )
{
	if ( trellis_job.status == TRELLIS_JOB_RUNNING ) {
		CL_MlWorker_Cancel( &s_trellisTask );
	}
	trellis_job.status = TRELLIS_JOB_IDLE;

	Cmd_RemoveCommand( "trellis_generate" );
	Cmd_RemoveCommand( "trellis_pipeline" );
	Cmd_RemoveCommand( "trellis_import" );
	Cmd_RemoveCommand( "trellis_status" );
	Cmd_RemoveCommand( "trellis_cancel" );
	Cmd_RemoveCommand( "trellis_show" );
	Cmd_RemoveCommand( "trellis_view" );
#if defined( USE_FLUX )
	Cmd_RemoveCommand( "trellis_from_prompt" );
#endif
}

#else

void CL_Trellis_Init( void ) { }
void CL_Trellis_Shutdown( void ) { }
void CL_Trellis_Frame( void ) { }

#endif /* USE_TRELLIS */
