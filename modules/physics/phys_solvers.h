/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Multi-solver registry on top of the Box3D Soft Step (or Bullet) substrate.
Secondary solvers read/write through Phys_* (ray/overlap/impulse) so they
share one rigid world without owning the Soft Step integrator.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "q_shared.h"

#define PHYS_SOLVER_MAX           16
#define PHYS_SOLVER_NAME_MAX      32

typedef enum {
	PHYS_SOLVER_PHASE_PRE_STEP = 0,  /* before Soft Step (rare) */
	PHYS_SOLVER_PHASE_POST_STEP,     /* after Soft Step — default */
	PHYS_SOLVER_PHASE_DEBUG
} physSolverPhase_t;

typedef struct physSolverDesc_s {
	char              name[PHYS_SOLVER_NAME_MAX];
	physSolverPhase_t phase;
	qboolean           enabled;
	void            ( *Init )( void );
	void            ( *Shutdown )( void );
	void            ( *Step )( float dt );
	void            ( *DebugDraw )( void );
	int             ( *GetActiveCount )( void );
} physSolverDesc_t;

void        PhysSolvers_Init( void );
void        PhysSolvers_Shutdown( void );
void        PhysSolvers_PreStep( float dt );   /* before Soft Step */
void        PhysSolvers_PostStep( float dt );  /* after Soft Step — companions */
void        PhysSolvers_Frame( float dt );     /* Pre+Post; prefer split around Soft Step */
void        PhysSolvers_DebugDraw( void );

/* Returns slot index or -1. name must be unique. */
int         PhysSolvers_Register( const physSolverDesc_t *desc );
qboolean    PhysSolvers_SetEnabled( const char *name, qboolean enabled );
qboolean    PhysSolvers_IsEnabled( const char *name );
int         PhysSolvers_GetCount( void );
const char *PhysSolvers_GetName( int index );
int         PhysSolvers_GetActiveCount( const char *name );

/* Built-in secondary solvers that collide against Box3D via Phys_*. */
void        PhysSolvers_RegisterBuiltins( void );

#ifdef __cplusplus
}
#endif
