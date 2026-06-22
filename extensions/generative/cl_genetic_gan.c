/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Client-side async GAN decode jobs (genome JSON -> mesh) and model import.
===========================================================================
*/

#include "client.h"
#include "cl_genetic_gan.h"
#include "cl_ml_worker.h"
#include "cl_pipeline.h"
#include "../qcommon/qcommon.h"
#include "../../world/genetic_gan.h"

#include <stdio.h>
#include <string.h>

#ifdef USE_GENETIC_GAN

#define GENOME_DECODE_QUEUE 8

cvar_t *cl_geneticGanAsync;
cvar_t *cl_geneticGanAutoImport;
cvar_t *cl_geneticGanRepo;
cvar_t *cl_geneticGanPython;
cvar_t *cl_geneticGanCmd;
cvar_t *cl_geneticGanTimeout;

typedef struct {
	geneticGanJobStatus_t status;
	qboolean notified;
	int start_time;
	int timeout_seconds;
	int slot;
	char genome_full[MAX_OSPATH];
	char genome_vfs[MAX_QPATH];
	char output_full[MAX_OSPATH];
	char output_vfs[MAX_QPATH];
	char repo[MAX_OSPATH];
	char error_msg[1024];
	qhandle_t model_handle;
} genetic_gan_job_t;

static genetic_gan_job_t genetic_gan_job;
static clMlTask_t s_decodeTask;
static int s_queue[GENOME_DECODE_QUEUE];
static int s_qCount;

static cvar_t *cl_geneticGanSyncQueue;
static char cl_geneticGanSyncQueueStr[16];

static const char *CL_GeneticGanDefaultCmd( void )
{
	return "%P \"%E/genetic_gan_decode.py\" --repo \"%R\" --genome \"%G\" --output \"%O\" --slot %S %A";
}

static qboolean CL_GeneticGanBuildExpand( cl_pipeline_expand_t *ex )
{
	const char *base;
	char slotStr[16];

	if ( !ex ) {
		return qfalse;
	}
	base = Sys_DefaultBasePath();
	Com_sprintf( slotStr, sizeof( slotStr ), "%d", genetic_gan_job.slot );
	Com_Memset( ex, 0, sizeof( *ex ) );
	ex->repo = genetic_gan_job.repo;
	ex->base = base ? base : "";
	ex->engine = base ? base : "";
	ex->py = ( cl_geneticGanPython && cl_geneticGanPython->string[0] ) ?
		cl_geneticGanPython->string : "python3";
	ex->genome = genetic_gan_job.genome_full;
	ex->output = genetic_gan_job.output_full;
	ex->slot = slotStr;
	ex->args = "";
	return qtrue;
}

static qhandle_t CL_GeneticGanImportVfs( const char *vfs_path )
{
	if ( !vfs_path || !vfs_path[0] || !re.RegisterModel ) {
		return 0;
	}
	return re.RegisterModel( vfs_path );
}

static void CL_GeneticGan_UpdateQueueSync( void )
{
	Com_sprintf( cl_geneticGanSyncQueueStr, sizeof( cl_geneticGanSyncQueueStr ), "%d", s_qCount );
}

static qboolean CL_GeneticGan_Enqueue( int slot )
{
	if ( s_qCount >= GENOME_DECODE_QUEUE ) {
		Com_Printf( S_COLOR_YELLOW "GeneticGAN: decode queue full (%d)\n", GENOME_DECODE_QUEUE );
		return qfalse;
	}
	s_queue[s_qCount++] = slot;
	CL_GeneticGan_UpdateQueueSync();
	Com_Printf( "GeneticGAN: queued decode slot %d (depth %d)\n", slot, s_qCount );
	return qtrue;
}

static void CL_GeneticGan_ClearQueue( void )
{
	s_qCount = 0;
	CL_GeneticGan_UpdateQueueSync();
}

static void CL_GeneticGan_MaybeAutoImport( void );
static qboolean CL_GeneticGan_BeginSlot( int slot );
static void CL_GeneticGan_PumpQueue( void );

static void CL_GeneticGan_Worker( void *data )
{
	genetic_gan_job_t *job = (genetic_gan_job_t *)data;
	cl_pipeline_expand_t ex;
	const char *tmpl;

	if ( !job ) {
		s_decodeTask.state = CL_ML_TASK_FAILED;
		return;
	}

	if ( !CL_GeneticGanBuildExpand( &ex ) ) {
		Q_strncpyz( job->error_msg, "Failed to build decode command", sizeof( job->error_msg ) );
		job->status = GENETIC_GAN_JOB_FAILED;
		s_decodeTask.state = CL_ML_TASK_FAILED;
		return;
	}
	tmpl = ( cl_geneticGanCmd && cl_geneticGanCmd->string[0] ) ?
		cl_geneticGanCmd->string : CL_GeneticGanDefaultCmd();
	if ( !CL_PipelineRunTemplate( tmpl, &ex, qtrue, "GeneticGAN" ) ) {
		Q_strncpyz( job->error_msg, "GAN decode subprocess failed", sizeof( job->error_msg ) );
		job->status = GENETIC_GAN_JOB_FAILED;
		s_decodeTask.state = CL_ML_TASK_FAILED;
		return;
	}
	if ( !CL_PipelineFileExists( job->output_full ) ) {
		Q_strncpyz( job->error_msg, "Output mesh missing after decode", sizeof( job->error_msg ) );
		job->status = GENETIC_GAN_JOB_FAILED;
		s_decodeTask.state = CL_ML_TASK_FAILED;
		return;
	}
	job->status = GENETIC_GAN_JOB_COMPLETED;
	s_decodeTask.state = CL_ML_TASK_SUCCEEDED;
}

static void CL_GeneticGan_DeferFinalize( void *data )
{
	genetic_gan_job_t *job = (genetic_gan_job_t *)data;

	if ( !job ) {
		CL_MlWorker_Release( "genetic_gan" );
		return;
	}

	if ( job->status == GENETIC_GAN_JOB_COMPLETED ) {
		GeneticGan_SetJobStatus( GENETIC_GAN_JOB_COMPLETED, NULL );
		CL_GeneticGan_MaybeAutoImport();
		Com_Printf( S_COLOR_GREEN "GeneticGAN: decode complete slot %d -> %s\n", job->slot, job->output_vfs );
	} else {
		GeneticGan_SetJobStatus( GENETIC_GAN_JOB_FAILED, job->error_msg );
		Com_Printf( S_COLOR_RED "GeneticGAN: decode failed: %s\n", job->error_msg );
	}

	job->notified = qtrue;
	genetic_gan_job.status = GENETIC_GAN_JOB_IDLE;
	CL_MlWorker_Release( "genetic_gan" );
	CL_GeneticGan_PumpQueue();
}

static qboolean CL_GeneticGan_BeginSlot( int slot )
{
	const char *base;
	const char *repo;

	if ( genetic_gan_job.status == GENETIC_GAN_JOB_RUNNING ) {
		return qfalse;
	}
	if ( !GeneticGan_IsActive( slot ) ) {
		Com_Printf( S_COLOR_YELLOW "GeneticGAN: invalid genome slot %d\n", slot );
		return qfalse;
	}

	repo = cl_geneticGanRepo ? cl_geneticGanRepo->string : "";
	base = Sys_DefaultBasePath();
	if ( !base ) {
		Com_Printf( S_COLOR_RED "GeneticGAN: no engine base path\n" );
		return qfalse;
	}

	Com_Memset( &genetic_gan_job, 0, sizeof( genetic_gan_job ) );
	genetic_gan_job.slot = slot;
	genetic_gan_job.start_time = Com_Milliseconds();
	genetic_gan_job.timeout_seconds = cl_geneticGanTimeout ? cl_geneticGanTimeout->integer : 1800;
	Q_strncpyz( genetic_gan_job.repo, repo ? repo : "", sizeof( genetic_gan_job.repo ) );
	Com_sprintf( genetic_gan_job.genome_vfs, sizeof( genetic_gan_job.genome_vfs ),
		"models/genome/genome_%d_%d.json", slot, Com_Milliseconds() );
	CL_PipelineResolvePath( base, genetic_gan_job.genome_vfs, genetic_gan_job.genome_full,
		sizeof( genetic_gan_job.genome_full ) );
	CL_PipelineEnsureOutputDir( genetic_gan_job.genome_full );
	if ( !GeneticGan_WriteGenomeJson( slot, genetic_gan_job.genome_full ) ) {
		Com_Printf( S_COLOR_RED "GeneticGAN: failed to write genome JSON\n" );
		return qfalse;
	}
	Com_sprintf( genetic_gan_job.output_vfs, sizeof( genetic_gan_job.output_vfs ),
		"models/genome/body_%d_%d.glb", slot, Com_Milliseconds() );
	CL_PipelineResolvePath( base, genetic_gan_job.output_vfs, genetic_gan_job.output_full,
		sizeof( genetic_gan_job.output_full ) );
	CL_PipelineEnsureOutputDir( genetic_gan_job.output_full );

	GeneticGan_SetJobSlot( slot );
	GeneticGan_SetJobStatus( GENETIC_GAN_JOB_RUNNING, NULL );
	genetic_gan_job.status = GENETIC_GAN_JOB_RUNNING;
	genetic_gan_job.notified = qfalse;

	if ( !cl_geneticGanAsync || !cl_geneticGanAsync->integer ) {
		CL_GeneticGan_Worker( &genetic_gan_job );
		CL_GeneticGan_DeferFinalize( &genetic_gan_job );
		return qtrue;
	}

	CL_MlWorker_InitTask( &s_decodeTask, "genetic_gan", CL_GeneticGan_Worker,
		CL_GeneticGan_DeferFinalize, &genetic_gan_job );

	if ( !CL_MlWorker_Submit( &s_decodeTask ) ) {
		genetic_gan_job.status = GENETIC_GAN_JOB_IDLE;
		GeneticGan_SetJobStatus( GENETIC_GAN_JOB_IDLE, NULL );
		if ( CL_MlWorker_IsBusy() ) {
			Com_Printf( S_COLOR_YELLOW "GeneticGAN: ML worker busy (%s), re-queue slot %d\n",
				CL_MlWorker_Owner(), slot );
			return qfalse;
		}
		Com_Printf( S_COLOR_RED "GeneticGAN: failed to start decode worker\n" );
		return qfalse;
	}

	Com_Printf( "GeneticGAN: started decode slot %d -> %s (jobs pool)\n", slot, genetic_gan_job.output_vfs );
	return qtrue;
}

static void CL_GeneticGan_PumpQueue( void )
{
	int slot;

	if ( s_qCount <= 0 ) {
		return;
	}
	if ( genetic_gan_job.status == GENETIC_GAN_JOB_RUNNING ) {
		return;
	}
	if ( CL_MlWorker_IsBusy() && Q_stricmp( CL_MlWorker_Owner(), "genetic_gan" ) != 0 ) {
		return;
	}

	slot = s_queue[0];
	if ( CL_GeneticGan_BeginSlot( slot ) ) {
		if ( s_qCount > 1 ) {
			memmove( s_queue, s_queue + 1, (size_t)( s_qCount - 1 ) * sizeof( s_queue[0] ) );
		}
		s_qCount--;
		CL_GeneticGan_UpdateQueueSync();
	}
}

static void CL_GeneticGan_MaybeAutoImport( void )
{
	if ( !cl_geneticGanAutoImport || !cl_geneticGanAutoImport->integer ) {
		return;
	}
	if ( genetic_gan_job.status != GENETIC_GAN_JOB_COMPLETED ) {
		return;
	}
	genetic_gan_job.model_handle = CL_GeneticGanImportVfs( genetic_gan_job.output_vfs );
	if ( genetic_gan_job.model_handle ) {
		GeneticGan_SetSlotOutput( genetic_gan_job.slot, genetic_gan_job.output_vfs );
		Com_Printf( S_COLOR_GREEN "GeneticGAN: imported %s (handle %d)\n",
			genetic_gan_job.output_vfs, genetic_gan_job.model_handle );
	} else {
		Com_Printf( S_COLOR_YELLOW "GeneticGAN: output at %s — genome_view %s\n",
			genetic_gan_job.output_vfs, genetic_gan_job.output_vfs );
	}
}

static qboolean CL_GeneticGanStartDecode( int slot )
{
	if ( genetic_gan_job.status == GENETIC_GAN_JOB_RUNNING ) {
		return CL_GeneticGan_Enqueue( slot );
	}
	if ( CL_MlWorker_IsBusy() && Q_stricmp( CL_MlWorker_Owner(), "genetic_gan" ) != 0 ) {
		return CL_GeneticGan_Enqueue( slot );
	}
	if ( CL_GeneticGan_BeginSlot( slot ) ) {
		return qtrue;
	}
	return CL_GeneticGan_Enqueue( slot );
}

static void CL_GeneticGanGenerate_f( void )
{
	int slot;

	if ( !GeneticGan_Enabled() ) {
		Com_Printf( S_COLOR_YELLOW "genome_generate: set cl_geneticGan 1\n" );
		return;
	}
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: genome_generate <slot>\n" );
		return;
	}
	slot = atoi( Cmd_Argv( 1 ) );
	CL_GeneticGanStartDecode( slot );
}

static void CL_GeneticGanDecodeStatus_f( void )
{
	switch ( genetic_gan_job.status ) {
	case GENETIC_GAN_JOB_IDLE:
		Com_Printf( "GeneticGAN decode: idle (queue %d)\n", s_qCount );
		break;
	case GENETIC_GAN_JOB_RUNNING: {
		int runtime = ( Com_Milliseconds() - genetic_gan_job.start_time ) / 1000;
		Com_Printf( "GeneticGAN decode: running slot %d (%ds) -> %s\n",
			genetic_gan_job.slot, runtime, genetic_gan_job.output_vfs );
		break;
	}
	case GENETIC_GAN_JOB_COMPLETED:
		Com_Printf( S_COLOR_GREEN "GeneticGAN decode: complete slot %d -> %s (handle %d)\n",
			genetic_gan_job.slot, genetic_gan_job.output_vfs, genetic_gan_job.model_handle );
		break;
	case GENETIC_GAN_JOB_FAILED:
		Com_Printf( S_COLOR_RED "GeneticGAN decode: failed: %s\n", genetic_gan_job.error_msg );
		break;
	}
}

static void CL_GeneticGanDecodeCancel_f( void )
{
	if ( genetic_gan_job.status == GENETIC_GAN_JOB_RUNNING ) {
		CL_MlWorker_Cancel( &s_decodeTask );
		Q_strncpyz( genetic_gan_job.error_msg, "Cancelled by user", sizeof( genetic_gan_job.error_msg ) );
		genetic_gan_job.status = GENETIC_GAN_JOB_IDLE;
		GeneticGan_SetJobStatus( GENETIC_GAN_JOB_IDLE, genetic_gan_job.error_msg );
		genetic_gan_job.notified = qtrue;
	}
	CL_GeneticGan_ClearQueue();
	CL_MlWorker_Release( "genetic_gan" );
	Com_Printf( "GeneticGAN decode: cancelled (queue cleared)\n" );
}

static void CL_GeneticGanView_f( void )
{
	const char *name;
	qhandle_t h;

	if ( Cmd_Argc() >= 2 ) {
		name = Cmd_Argv( 1 );
	} else if ( genetic_gan_job.status == GENETIC_GAN_JOB_COMPLETED && genetic_gan_job.output_vfs[0] ) {
		name = genetic_gan_job.output_vfs;
	} else {
		Com_Printf( "Usage: genome_view [models/genome/body_N.glb]\n" );
		return;
	}
	h = CL_GeneticGanImportVfs( name );
	if ( h ) {
		Com_Printf( S_COLOR_GREEN "GeneticGAN: registered %s (handle %d)\n", name, h );
		genetic_gan_job.model_handle = h;
	} else {
		Com_Printf( S_COLOR_YELLOW "GeneticGAN: RegisterModel failed for %s\n", name );
	}
}

void CL_GeneticGan_Frame( void )
{
	if ( genetic_gan_job.status == GENETIC_GAN_JOB_RUNNING && genetic_gan_job.timeout_seconds > 0 ) {
		int runtime = ( Com_Milliseconds() - genetic_gan_job.start_time ) / 1000;
		if ( runtime > genetic_gan_job.timeout_seconds ) {
			Com_Printf( S_COLOR_YELLOW "GeneticGAN: decode exceeded %d s (still running); genome_decode_cancel to stop\n",
				genetic_gan_job.timeout_seconds );
			genetic_gan_job.timeout_seconds = 0;
		}
	}
	CL_GeneticGan_PumpQueue();
}

void CL_GeneticGan_Init( void )
{
	GeneticGan_Init();

	cl_geneticGanAsync = Cvar_Get( "cl_geneticGanAsync", "1", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_geneticGanAsync, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_geneticGanAsync,
		"Genome GAN decode: 0=blocking console, 1=engine job pool / ML worker (see cl_mlUseJobs)." );

	cl_geneticGanSyncQueue = Cvar_Get( "cl_geneticGanSyncQueue", "0", CVAR_ROM );
	cl_geneticGanSyncQueue->string = cl_geneticGanSyncQueueStr;
	CL_GeneticGan_UpdateQueueSync();

	cl_geneticGanAutoImport = Cvar_Get( "cl_geneticGanAutoImport", "1", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_geneticGanAutoImport, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_geneticGanAutoImport,
		"When 1, RegisterModel on decode completion." );

	cl_geneticGanTimeout = Cvar_Get( "cl_geneticGanTimeout", "1800", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_geneticGanTimeout, "60", "86400", CV_INTEGER );
	Cvar_SetDescription( cl_geneticGanTimeout, "Warn if background decode exceeds this many seconds." );

	cl_geneticGanRepo = Cvar_Get( "cl_geneticGanRepo", "", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_geneticGanRepo,
		"Path to external genetic/GAN body synthesis checkout (%R in cl_geneticGanCmd)." );

	cl_geneticGanPython = Cvar_Get( "cl_geneticGanPython", "python3", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_geneticGanPython, "Python interpreter (%P) for genome decode wrapper." );

	cl_geneticGanCmd = Cvar_Get( "cl_geneticGanCmd", "", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_geneticGanCmd,
		"Shell template for genome_generate. Tokens: %R repo, %B base, %E release, %P python, %G genome json, %O output glb, %S slot, %A args." );

	Cmd_AddCommand( "genome_generate", CL_GeneticGanGenerate_f );
	Cmd_AddCommand( "genome_decode_status", CL_GeneticGanDecodeStatus_f );
	Cmd_AddCommand( "genome_decode_cancel", CL_GeneticGanDecodeCancel_f );
	Cmd_AddCommand( "genome_view", CL_GeneticGanView_f );

	Com_Memset( &genetic_gan_job, 0, sizeof( genetic_gan_job ) );

	if ( GeneticGan_Enabled() ) {
		Com_Printf( "Genetic GAN ML: enabled (async %s, auto_import %s; docs/GENETIC_GAN.md)\n",
			( cl_geneticGanAsync && cl_geneticGanAsync->integer ) ? "on" : "off",
			( cl_geneticGanAutoImport && cl_geneticGanAutoImport->integer ) ? "on" : "off" );
	}
}

void CL_GeneticGan_Shutdown( void )
{
	CL_MlWorker_Cancel( &s_decodeTask );
	CL_GeneticGan_ClearQueue();
	Cmd_RemoveCommand( "genome_generate" );
	Cmd_RemoveCommand( "genome_decode_status" );
	Cmd_RemoveCommand( "genome_decode_cancel" );
	Cmd_RemoveCommand( "genome_view" );
	GeneticGan_Shutdown();
}

#else

void CL_GeneticGan_Init( void ) { }
void CL_GeneticGan_Shutdown( void ) { }
void CL_GeneticGan_Frame( void ) { }

#endif
