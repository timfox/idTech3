/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#pragma once

#include "../qcommon/q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	CL_FLUX_JOB_IDLE,
	CL_FLUX_JOB_RUNNING,
	CL_FLUX_JOB_COMPLETED,
	CL_FLUX_JOB_FAILED
} clFluxJobStatus_t;

#ifdef USE_FLUX

extern cvar_t *cl_flux_enable;
extern cvar_t *cl_flux_async;
extern cvar_t *cl_flux_external;
extern cvar_t *cl_flux_model;
extern cvar_t *cl_flux_device;
extern cvar_t *cl_flux_width;
extern cvar_t *cl_flux_height;
extern cvar_t *cl_flux_steps;
extern cvar_t *cl_flux_seed;
extern cvar_t *cl_fonts_enable;
extern cvar_t *cl_fonts_repo;
extern cvar_t *cl_fonts_python;
extern cvar_t *cl_fonts_cmd;

void CL_Flux_Init( void );
void CL_Flux_Shutdown( void );
void CL_Flux_Frame( void );

clFluxJobStatus_t CL_Flux_GetJobStatus( void );
const char       *CL_Flux_GetOutputPath( void );
qboolean          CL_Flux_IsRunning( void );

void     CL_Flux_ArmTrellisChain( void );
qboolean CL_Flux_IsTrellisChainArmed( void );
void     CL_Flux_ClearTrellisChain( void );

#else

static inline void CL_Flux_Init( void ) {}
static inline void CL_Flux_Shutdown( void ) {}

#endif

#ifdef __cplusplus
}
#endif
