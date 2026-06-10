/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Serial ML / subprocess worker gate + async dispatch via the engine job pool
(or SDL thread fallback). Worker threads must not touch renderer or cvars;
use Defer_Add for main-thread finalization.
===========================================================================
*/

#pragma once

#include "../qcommon/q_shared.h"
#include "../qcommon/jobs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*clMlWorkerFn_t)( void *data );
typedef void (*clMlDeferFn_t)( void *data );

typedef enum {
	CL_ML_TASK_IDLE = 0,
	CL_ML_TASK_RUNNING,
	CL_ML_TASK_SUCCEEDED,
	CL_ML_TASK_FAILED
} clMlTaskState_t;

typedef struct clMlTask_s {
	char                name[32];
	clMlWorkerFn_t      worker;
	clMlDeferFn_t       defer;
	void                *data;
	clMlTaskState_t     state;
	jobHandle_t         jobHandle;
#if USE_SDL
	struct SDL_Thread   *sdlThread;
#endif
} clMlTask_t;

void        CL_MlWorker_Init( void );
void        CL_MlWorker_Shutdown( void );

qboolean    CL_MlWorker_IsBusy( void );
const char *CL_MlWorker_Owner( void );

qboolean    CL_MlWorker_Submit( clMlTask_t *task );
void        CL_MlWorker_Frame( void );

void        CL_MlWorker_InitTask( clMlTask_t *task, const char *name, clMlWorkerFn_t worker,
                clMlDeferFn_t defer, void *data );

void        CL_MlWorker_Cancel( clMlTask_t *task );
void        CL_MlWorker_Release( const char *name );

#ifdef __cplusplus
}
#endif
