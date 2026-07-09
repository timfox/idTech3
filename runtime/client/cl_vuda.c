/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Client-side VUDA scheduler: imports Vulkan exports and runs CUDA in render gaps.
===========================================================================
*/

#include "client.h"
#include "cl_vuda.h"

#ifdef USE_VUDA

#include "vuda/vuda_cuda.h"
#include "../renderers/common/tr_public.h"

static cvar_t *cl_vuda;
static cvar_t *cl_vuda_auto_job;

static qboolean cl_vuda_ready;
static int cl_vuda_last_import_frame;
static qboolean cl_vuda_step_pending;
static uint64_t cl_vuda_pending_signal_timeline;

static int CL_VUDA_ParseJobKind( const char *name )
{
	if ( !name || !name[0] || !Q_stricmp( name, "neural" ) ) {
		return VUDA_JOB_NEURAL_STAGE;
	}
	if ( !Q_stricmp( name, "heartbeat" ) ) {
		return VUDA_JOB_HEARTBEAT;
	}
	if ( !Q_stricmp( name, "physics" ) ) {
		return VUDA_JOB_PHYSICS_TICK;
	}
	if ( !Q_stricmp( name, "inference" ) ) {
		return VUDA_JOB_INFERENCE;
	}
	return VUDA_JOB_NEURAL_STAGE;
}

static void CL_VUDA_Cmd_Status( void )
{
	vudaExportBundle_t exp;
	qboolean interop = re.VudaInteropReady && re.VudaInteropReady();

	Com_Printf( "[VUDA] cl_vuda=%d ready=%d cuda=%s interop=%d active=%d bound_streams=%d\n",
		cl_vuda ? cl_vuda->integer : 0,
		cl_vuda_ready ? 1 : 0,
		VudaCuda_BackendName(),
		interop ? 1 : 0,
		( re.VudaActive && re.VudaActive() ) ? 1 : 0,
		VudaCuda_BoundStreamCount() );

	if ( re.VudaGetExportBundle && re.VudaGetExportBundle( &exp ) ) {
		int i;
		Com_Printf( "[VUDA] slots: " );
		for ( i = 0; i < VUDA_MAX_SLOTS; i++ ) {
			if ( exp.slots[i].valid ) {
				Com_Printf( "%d=%lluKB ", i, (unsigned long long)( exp.slots[i].size / 1024 ) );
			}
		}
		Com_Printf( "timeline render=%llu cuda=%llu\n",
			(unsigned long long)exp.renderTimeline,
			(unsigned long long)exp.cudaTimeline );
	}
}

static void CL_VUDA_Cmd_Reload( void )
{
	vudaExportBundle_t exp;

	if ( !re.VudaInteropReady || !re.VudaInteropReady() ) {
		Com_Printf( S_COLOR_YELLOW "[VUDA] Renderer interop not ready (r_vuda 1 + vid_restart)\n" );
		return;
	}
	if ( !re.VudaGetExportBundle( &exp ) || !VudaCuda_ImportExports( &exp ) ) {
		Com_Printf( S_COLOR_YELLOW "[VUDA] Failed to import Vulkan exports into CUDA\n" );
		cl_vuda_ready = qfalse;
		return;
	}
	cl_vuda_ready = qtrue;
	Com_Printf( "[VUDA] CUDA import OK (%s)\n", VudaCuda_BackendName() );
}

static void CL_VUDA_Cmd_Run( void )
{
	vudaCudaJob_t job;
	vudaExportBundle_t exp;
	int maxMs;
	uint64_t signalTimeline;

	if ( !cl_vuda_ready ) {
		CL_VUDA_Cmd_Reload();
		if ( !cl_vuda_ready ) {
			return;
		}
	}

	if ( !re.VudaGetExportBundle || !re.VudaGetExportBundle( &exp ) ) {
		Com_Printf( S_COLOR_YELLOW "[VUDA] Export bundle unavailable\n" );
		return;
	}

	Com_Memset( &job, 0, sizeof( job ) );
	job.kind = VUDA_JOB_HEARTBEAT;
	job.streamMask = 7;
	job.bytes = 1024;
	maxMs = Cvar_VariableIntegerValue( "r_vuda_computeMs" );
	if ( maxMs < 1 ) {
		maxMs = 2;
	}

	if ( VudaCuda_RunJob( &job, maxMs, exp.renderTimeline, &signalTimeline ) ) {
		Com_Printf( "[VUDA] Heartbeat job OK (timeline=%llu)\n",
			(unsigned long long)signalTimeline );
	} else {
		Com_Printf( S_COLOR_YELLOW "[VUDA] Heartbeat job failed\n" );
	}
}

static void CL_VUDA_Cmd_StepAsync( void )
{
	vudaCudaJob_t job;
	vudaExportBundle_t exp;
	int maxMs;

	if ( !cl_vuda_ready ) {
		CL_VUDA_Cmd_Reload();
		if ( !cl_vuda_ready ) {
			return;
		}
	}

	if ( !re.VudaGetExportBundle || !re.VudaGetExportBundle( &exp ) ) {
		Com_Printf( S_COLOR_YELLOW "[VUDA] Export bundle unavailable\n" );
		return;
	}

	Com_Memset( &job, 0, sizeof( job ) );
	job.kind = ( Cmd_Argc() >= 2 ) ? CL_VUDA_ParseJobKind( Cmd_Argv( 1 ) ) : VUDA_JOB_NEURAL_STAGE;
	job.streamMask = (uint32_t)Cvar_VariableIntegerValue( "r_vuda_coStreamMask" );
	job.bytes = ( Cmd_Argc() >= 3 ) ? (uint32_t)atoi( Cmd_Argv( 2 ) ) : 2048u;
	maxMs = Cvar_VariableIntegerValue( "r_vuda_computeMs" );
	if ( maxMs < 1 ) {
		maxMs = 2;
	}

	if ( !VudaCuda_RunJob( &job, maxMs, exp.renderTimeline, &cl_vuda_pending_signal_timeline ) ) {
		Com_Printf( S_COLOR_YELLOW "[VUDA] step_async failed\n" );
		cl_vuda_step_pending = qfalse;
		return;
	}

	cl_vuda_step_pending = qtrue;
	Com_Printf( "[VUDA] step_async queued kind=%d render=%llu signal=%llu\n",
		job.kind,
		(unsigned long long)exp.renderTimeline,
		(unsigned long long)cl_vuda_pending_signal_timeline );
}

static void CL_VUDA_Cmd_WaitStep( void )
{
	if ( !cl_vuda_step_pending ) {
		Com_Printf( "[VUDA] wait_step: no pending CUDA step\n" );
		return;
	}

	if ( re.VudaNotifyCudaComplete ) {
		re.VudaNotifyCudaComplete( cl_vuda_pending_signal_timeline );
	}
	Com_Printf( "[VUDA] wait_step completed (cuda timeline=%llu)\n",
		(unsigned long long)cl_vuda_pending_signal_timeline );
	cl_vuda_step_pending = qfalse;
}

static void CL_VUDA_Cmd_WaitRender( void )
{
	vudaExportBundle_t exp;

	if ( !cl_vuda_ready ) {
		CL_VUDA_Cmd_Reload();
		if ( !cl_vuda_ready ) {
			return;
		}
	}

	if ( !re.VudaGetExportBundle || !re.VudaGetExportBundle( &exp ) ) {
		Com_Printf( S_COLOR_YELLOW "[VUDA] Export bundle unavailable\n" );
		return;
	}

	if ( VudaCuda_WaitRenderTimeline( exp.renderTimeline ) ) {
		Com_Printf( "[VUDA] wait_render completed (render timeline=%llu)\n",
			(unsigned long long)exp.renderTimeline );
	} else {
		Com_Printf( S_COLOR_YELLOW "[VUDA] wait_render failed\n" );
	}
}

static void CL_VUDA_Cmd_BindStream_f( void )
{
	int slot;

	slot = ( Cmd_Argc() >= 2 ) ? atoi( Cmd_Argv( 1 ) ) : VUDA_STREAM_PHYSICS;
	if ( !VudaCuda_BindStream( slot ) ) {
		Com_Printf( S_COLOR_YELLOW "[VUDA] bind failed (slot 0=physics 1=neural 2=inference)\n" );
	}
}

static void CL_VUDA_Cmd_UnbindStream_f( void )
{
	int slot;

	slot = ( Cmd_Argc() >= 2 ) ? atoi( Cmd_Argv( 1 ) ) : VUDA_STREAM_PHYSICS;
	(void)VudaCuda_UnbindStream( slot );
}

void CL_VUDA_Init( void )
{
	cl_vuda = Cvar_Get( "cl_vuda", "0", CVAR_ARCHIVE );
	cl_vuda_auto_job = Cvar_Get( "cl_vuda_auto_job", "1", CVAR_ARCHIVE );

	Cvar_CheckRange( cl_vuda, "0", "1", CV_INTEGER );
	Cvar_CheckRange( cl_vuda_auto_job, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_vuda,
		"Client VUDA scheduler: import Vulkan shared buffers and run CUDA in render gaps." );
	Cvar_SetDescription( cl_vuda_auto_job,
		"Run heartbeat CUDA job each frame when compute window is open." );

	Cmd_AddCommand( "vuda_status", CL_VUDA_Cmd_Status );
	Cmd_AddCommand( "vuda_reload", CL_VUDA_Cmd_Reload );
	Cmd_AddCommand( "vuda_run", CL_VUDA_Cmd_Run );
	Cmd_AddCommand( "vuda_step_async", CL_VUDA_Cmd_StepAsync );
	Cmd_AddCommand( "vuda_wait_step", CL_VUDA_Cmd_WaitStep );
	Cmd_AddCommand( "vuda_wait_render", CL_VUDA_Cmd_WaitRender );
	Cmd_AddCommand( "vuda_bind_stream", CL_VUDA_Cmd_BindStream_f );
	Cmd_AddCommand( "vuda_unbind_stream", CL_VUDA_Cmd_UnbindStream_f );

	if ( !VudaCuda_Init() ) {
		Com_Printf( S_COLOR_YELLOW "[VUDA] CUDA unavailable — install NVIDIA driver + libcudart\n" );
	}

	if ( cl_vuda->integer ) {
		Com_Printf( "[VUDA] Client scheduler enabled (requires r_vuda 1 + vid_restart)\n" );
	}
}

void CL_VUDA_Shutdown( void )
{
	Cmd_RemoveCommand( "vuda_status" );
	Cmd_RemoveCommand( "vuda_reload" );
	Cmd_RemoveCommand( "vuda_run" );
	Cmd_RemoveCommand( "vuda_step_async" );
	Cmd_RemoveCommand( "vuda_wait_step" );
	Cmd_RemoveCommand( "vuda_wait_render" );
	Cmd_RemoveCommand( "vuda_bind_stream" );
	Cmd_RemoveCommand( "vuda_unbind_stream" );
	VudaCuda_Shutdown();
	cl_vuda_ready = qfalse;
	cl_vuda_step_pending = qfalse;
}

void CL_VUDA_Frame( void )
{
	vudaExportBundle_t exp;
	vudaCudaJob_t job;
	int maxMs;
	uint64_t signalTimeline;

	if ( !cl_vuda || !cl_vuda->integer ) {
		return;
	}
	if ( !re.VudaActive || !re.VudaActive() ) {
		return;
	}
	if ( !VudaCuda_Available() ) {
		return;
	}

	if ( !cl_vuda_ready ) {
		if ( cls.framecount - cl_vuda_last_import_frame > 60 ) {
			cl_vuda_last_import_frame = cls.framecount;
			CL_VUDA_Cmd_Reload();
		}
		if ( !cl_vuda_ready ) {
			return;
		}
	}

	if ( !re.VudaConsumeComputeWindow || !re.VudaConsumeComputeWindow() ) {
		return;
	}

	if ( !cl_vuda_auto_job || !cl_vuda_auto_job->integer ) {
		return;
	}

	if ( !re.VudaGetExportBundle || !re.VudaGetExportBundle( &exp ) ) {
		return;
	}

	Com_Memset( &job, 0, sizeof( job ) );
	job.kind = VUDA_JOB_NEURAL_STAGE;
	job.streamMask = (uint32_t)Cvar_VariableIntegerValue( "r_vuda_coStreamMask" );
	job.bytes = 2048;
	maxMs = Cvar_VariableIntegerValue( "r_vuda_computeMs" );
	if ( maxMs < 1 ) {
		maxMs = 2;
	}

	if ( VudaCuda_RunJob( &job, maxMs, exp.renderTimeline, &signalTimeline ) ) {
		if ( re.VudaNotifyCudaComplete ) {
			re.VudaNotifyCudaComplete( signalTimeline );
		}
		cl_vuda_step_pending = qfalse;
	}
}

#else

void CL_VUDA_Init( void ) {}
void CL_VUDA_Shutdown( void ) {}
void CL_VUDA_Frame( void ) {}

#endif
