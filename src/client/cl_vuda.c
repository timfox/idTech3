/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Client-side VUDA scheduler: imports Vulkan exports and runs CUDA in render gaps.
===========================================================================
*/

#include "client.h"
#include "cl_vuda.h"

#ifdef USE_VUDA

#include "../vuda/vuda_cuda.h"
#include "../renderers/common/tr_public.h"

static cvar_t *cl_vuda;
static cvar_t *cl_vuda_auto_job;

static qboolean cl_vuda_ready;
static int cl_vuda_last_import_frame;

static void CL_VUDA_Cmd_Status( void )
{
	vudaExportBundle_t exp;
	qboolean interop = re.VudaInteropReady && re.VudaInteropReady();

	Com_Printf( "[VUDA] cl_vuda=%d ready=%d cuda=%s interop=%d active=%d\n",
		cl_vuda ? cl_vuda->integer : 0,
		cl_vuda_ready ? 1 : 0,
		VudaCuda_BackendName(),
		interop ? 1 : 0,
		( re.VudaActive && re.VudaActive() ) ? 1 : 0 );

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
	int maxMs;

	if ( !cl_vuda_ready ) {
		CL_VUDA_Cmd_Reload();
		if ( !cl_vuda_ready ) {
			return;
		}
	}

	Com_Memset( &job, 0, sizeof( job ) );
	job.kind = VUDA_JOB_HEARTBEAT;
	job.streamMask = 7;
	job.bytes = 1024;
	maxMs = Cvar_VariableIntegerValue( "r_vuda_computeMs" );
	if ( maxMs < 1 ) {
		maxMs = 2;
	}

	if ( VudaCuda_RunJob( &job, maxMs ) ) {
		Com_Printf( "[VUDA] Heartbeat job OK\n" );
	} else {
		Com_Printf( S_COLOR_YELLOW "[VUDA] Heartbeat job failed\n" );
	}
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
	VudaCuda_Shutdown();
	cl_vuda_ready = qfalse;
}

void CL_VUDA_Frame( void )
{
	vudaExportBundle_t exp;
	vudaCudaJob_t job;
	int maxMs;

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

	Com_Memset( &job, 0, sizeof( job ) );
	job.kind = VUDA_JOB_NEURAL_STAGE;
	job.streamMask = (uint32_t)Cvar_VariableIntegerValue( "r_vuda_coStreamMask" );
	job.bytes = 2048;
	maxMs = Cvar_VariableIntegerValue( "r_vuda_computeMs" );
	if ( maxMs < 1 ) {
		maxMs = 2;
	}

	if ( VudaCuda_RunJob( &job, maxMs ) && re.VudaGetExportBundle( &exp ) ) {
		if ( re.VudaNotifyCudaComplete ) {
			re.VudaNotifyCudaComplete( exp.cudaTimeline + 1 );
		}
	}
}

#else

void CL_VUDA_Init( void ) {}
void CL_VUDA_Shutdown( void ) {}
void CL_VUDA_Frame( void ) {}

#endif
