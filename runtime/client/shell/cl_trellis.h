/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#pragma once

#include "q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef USE_TRELLIS

extern cvar_t *cl_trellis_enable;
extern cvar_t *cl_trellis_async;
extern cvar_t *cl_trellis_auto_import;
extern cvar_t *cl_trellis_chain;
extern cvar_t *cl_trellis_repo;
extern cvar_t *cl_trellis_python;
extern cvar_t *cl_trellis_conda;
extern cvar_t *cl_trellis_cmd;
extern cvar_t *cl_trellis_hf_model;
extern cvar_t *cl_trellis_decimation;
extern cvar_t *cl_trellis_texture_size;
extern cvar_t *cl_trellis_timeout;

void CL_Trellis_Init( void );
void CL_Trellis_Shutdown( void );
void CL_Trellis_Frame( void );
qboolean CL_Trellis_StartJob( const char *image_rel, const char *output_rel_optional );

#else

static inline void CL_Trellis_Init( void ) {}
static inline void CL_Trellis_Shutdown( void ) {}
static inline void CL_Trellis_Frame( void ) {}

#endif

#ifdef __cplusplus
}
#endif
