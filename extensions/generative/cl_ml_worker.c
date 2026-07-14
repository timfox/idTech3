/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "client.h"
#include "cl_ml_worker.h"
#include "defer.h"
#include "jobs.h"

#ifndef _MSC_VER
#include <sched.h>
#endif

#if USE_SDL
#include <SDL3/SDL.h>
#else
typedef struct SDL_Thread SDL_Thread;
#endif

static cvar_t *cl_mlSerial;
static cvar_t *cl_mlUseJobs;

static qboolean s_mlBusy;
static char s_mlOwner[32];

#if USE_SDL
static SDL_SpinLock s_mlGate;
#endif

/*
===============
CL_MlWorker_InitTask
===============
*/
void CL_MlWorker_InitTask( clMlTask_t *task, const char *name, clMlWorkerFn_t worker,
	clMlDeferFn_t defer, void *data )
{
	if ( !task ) {
		return;
	}

	*task = (clMlTask_t){
		.worker = worker,
		.defer = defer,
		.data = data,
		.state = CL_ML_TASK_IDLE,
		.jobHandle = JOBS_INVALID_HANDLE,
#if USE_SDL
		.sdlThread = NULL,
#endif
	};

	if ( name ) {
		Q_strncpyz( task->name, name, sizeof( task->name ) );
	} else {
		task->name[0] = '\0';
	}
}

/*
===============
CL_MlWorker_RunTaskBody
Worker / SDL thread entry — no renderer or cvar access; defer for main thread.
===============
*/
static void CL_MlWorker_RunTaskBody( clMlTask_t *task )
{
	if ( !task || !task->worker ) {
		if ( task ) {
			task->state = CL_ML_TASK_FAILED;
		}
		return;
	}

	task->state = CL_ML_TASK_RUNNING;
	task->worker( task->data );

	if ( task->state == CL_ML_TASK_RUNNING ) {
		task->state = CL_ML_TASK_SUCCEEDED;
	}

	if ( task->defer ) {
		Defer_Add( task->defer, task->data );
	} else {
		CL_MlWorker_Release( task->name );
	}
}

static void CL_MlWorker_JobThunk( void *data, uint32_t count )
{
	(void)count;
	CL_MlWorker_RunTaskBody( (clMlTask_t *)data );
}

void CL_MlWorker_Release( const char *name )
{
	(void)name;
	s_mlBusy = qfalse;
	s_mlOwner[0] = '\0';
}

#if USE_SDL
static int CL_MlWorker_SdlThread( void *userdata )
{
	clMlTask_t *task = (clMlTask_t *)userdata;

	if ( !task ) {
		return -1;
	}

	CL_MlWorker_RunTaskBody( task );
	return 0;
}
#endif

void CL_MlWorker_Init( void )
{
	cl_mlSerial = Cvar_Get( "cl_mlSerial", "1", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_mlSerial, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_mlSerial,
		"When 1, only one ML/subprocess pipeline (FLUX/TRELLIS/genome decode) runs at a time." );

	cl_mlUseJobs = Cvar_Get( "cl_mlUseJobs", "1", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_mlUseJobs, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_mlUseJobs,
		"When 1 and jobs_enabled 1, ML worker tasks use the engine job thread pool instead of a dedicated SDL thread." );

	s_mlBusy = qfalse;
	s_mlOwner[0] = '\0';
#if USE_SDL
	s_mlGate = 0;
#endif

	Com_Printf( "ML worker: serial=%s use_jobs=%s (jobs_enabled %s, workers %d)\n",
		( cl_mlSerial && cl_mlSerial->integer ) ? "on" : "off",
		( cl_mlUseJobs && cl_mlUseJobs->integer ) ? "on" : "off",
		Cvar_VariableIntegerValue( "jobs_enabled" ) ? "on" : "off",
		Jobs_WorkerCount() );
}

void CL_MlWorker_Shutdown( void )
{
	s_mlBusy = qfalse;
	s_mlOwner[0] = '\0';
}

qboolean CL_MlWorker_IsBusy( void )
{
	return s_mlBusy;
}

const char *CL_MlWorker_Owner( void )
{
	return s_mlOwner;
}

qboolean CL_MlWorker_Submit( clMlTask_t *task )
{
	if ( !task || !task->worker ) {
		return qfalse;
	}

	if ( cl_mlSerial && cl_mlSerial->integer && s_mlBusy ) {
		return qfalse;
	}

#if USE_SDL
	if ( cl_mlSerial && cl_mlSerial->integer ) {
		SDL_LockSpinlock( &s_mlGate );
		if ( s_mlBusy ) {
			SDL_UnlockSpinlock( &s_mlGate );
			return qfalse;
		}
		s_mlBusy = qtrue;
		Q_strncpyz( s_mlOwner, task->name, sizeof( s_mlOwner ) );
		SDL_UnlockSpinlock( &s_mlGate );
	} else {
		s_mlBusy = qtrue;
		Q_strncpyz( s_mlOwner, task->name, sizeof( s_mlOwner ) );
	}
#else
	s_mlBusy = qtrue;
	Q_strncpyz( s_mlOwner, task->name, sizeof( s_mlOwner ) );
#endif

	task->state = CL_ML_TASK_RUNNING;
	task->jobHandle = JOBS_INVALID_HANDLE;
#if USE_SDL
	task->sdlThread = NULL;
#endif

	if ( cl_mlUseJobs && cl_mlUseJobs->integer && Cvar_VariableIntegerValue( "jobs_enabled" ) ) {
		task->jobHandle = Jobs_SubmitWork( CL_MlWorker_JobThunk, task, JOB_PRIORITY_NORMAL );
		if ( task->jobHandle != JOBS_INVALID_HANDLE ) {
			return qtrue;
		}
		Com_Printf( S_COLOR_YELLOW "ML worker: Jobs_Submit failed for '%s', falling back to SDL thread\n",
			task->name );
	}

#if USE_SDL
	task->sdlThread = SDL_CreateThread( CL_MlWorker_SdlThread, task->name, task );
	if ( !task->sdlThread ) {
		task->state = CL_ML_TASK_FAILED;
		s_mlBusy = qfalse;
		s_mlOwner[0] = '\0';
		Com_Printf( S_COLOR_RED "ML worker: failed to create thread for '%s'\n", task->name );
		return qfalse;
	}
	return qtrue;
#else
	task->state = CL_ML_TASK_FAILED;
	s_mlBusy = qfalse;
	s_mlOwner[0] = '\0';
	Com_Printf( S_COLOR_RED "ML worker: no job pool or SDL threads for '%s'\n", task->name );
	return qfalse;
#endif
}

void CL_MlWorker_Frame( void )
{
	if ( s_mlBusy && Jobs_WorkerCount() > 0 ) {
		Jobs_Pump( 2 );
	}
}

void CL_MlWorker_Cancel( clMlTask_t *task )
{
	if ( !task ) {
		return;
	}

#if USE_SDL
	if ( task->sdlThread ) {
		int rc = 0;
		SDL_WaitThread( task->sdlThread, &rc );
		task->sdlThread = NULL;
		(void)rc;
	}
#endif

	if ( task->jobHandle != JOBS_INVALID_HANDLE ) {
		Jobs_Wait( task->jobHandle );
		task->jobHandle = JOBS_INVALID_HANDLE;
	}

	if ( task->state == CL_ML_TASK_RUNNING ) {
		task->state = CL_ML_TASK_FAILED;
	}

	if ( s_mlBusy && ( !task->name[0] || !Q_stricmp( s_mlOwner, task->name ) ) ) {
		s_mlBusy = qfalse;
		s_mlOwner[0] = '\0';
	}
}
