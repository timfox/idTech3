/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "phys_solvers.h"
#include "phys_cloth.h"
#include "phys_particles.h"
#include "phys_softblob.h"
#include "phys_fluid.h"
#include "phys_debugdraw.h"
#include "phys_bullet.h"
#include "phys_props.h"
#include "phys_volumes.h"
#include "phys_motor.h"
#include "phys_procedural_anim.h"

#include <string.h>

typedef struct {
	physSolverDesc_t desc;
	qboolean         used;
} physSolverSlot_t;

static physSolverSlot_t s_solvers[PHYS_SOLVER_MAX];
static int s_solverCount;
static qboolean s_ready;

static int PhysSolvers_Find( const char *name ) {
	int i;
	if ( !name || !name[0] ) {
		return -1;
	}
	for ( i = 0; i < s_solverCount; i++ ) {
		if ( s_solvers[i].used && !Q_stricmp( s_solvers[i].desc.name, name ) ) {
			return i;
		}
	}
	return -1;
}

void PhysSolvers_Init( void ) {
	if ( s_ready ) {
		return;
	}
	Com_Memset( s_solvers, 0, sizeof( s_solvers ) );
	s_solverCount = 0;
	s_ready = qtrue;
	PhysSolvers_RegisterBuiltins();
	Com_Printf( "[physics] multi-solver registry ready (%d solvers on Soft Step substrate)\n",
		s_solverCount );
}

void PhysSolvers_Shutdown( void ) {
	int i;
	if ( !s_ready ) {
		return;
	}
	for ( i = 0; i < s_solverCount; i++ ) {
		if ( s_solvers[i].used && s_solvers[i].desc.Shutdown ) {
			s_solvers[i].desc.Shutdown();
		}
	}
	Com_Memset( s_solvers, 0, sizeof( s_solvers ) );
	s_solverCount = 0;
	s_ready = qfalse;
}

int PhysSolvers_Register( const physSolverDesc_t *desc ) {
	int idx;
	physSolverSlot_t *slot;

	if ( !s_ready || !desc || !desc->name[0] ) {
		return -1;
	}
	if ( PhysSolvers_Find( desc->name ) >= 0 ) {
		Com_Printf( S_COLOR_YELLOW "PhysSolvers: duplicate '%s'\n", desc->name );
		return -1;
	}
	if ( s_solverCount >= PHYS_SOLVER_MAX ) {
		return -1;
	}

	idx = s_solverCount++;
	slot = &s_solvers[idx];
	slot->desc = *desc;
	slot->used = qtrue;
	if ( slot->desc.Init ) {
		slot->desc.Init();
	}
	Com_Printf( "[physics] solver '%s' registered (phase=%d enabled=%d)\n",
		slot->desc.name, (int)slot->desc.phase, slot->desc.enabled ? 1 : 0 );
	return idx;
}

qboolean PhysSolvers_SetEnabled( const char *name, qboolean enabled ) {
	int idx = PhysSolvers_Find( name );
	if ( idx < 0 ) {
		return qfalse;
	}
	s_solvers[idx].desc.enabled = enabled;
	return qtrue;
}

qboolean PhysSolvers_IsEnabled( const char *name ) {
	int idx = PhysSolvers_Find( name );
	if ( idx < 0 ) {
		return qfalse;
	}
	return s_solvers[idx].desc.enabled;
}

int PhysSolvers_GetCount( void ) {
	return s_solverCount;
}

const char *PhysSolvers_GetName( int index ) {
	if ( index < 0 || index >= s_solverCount || !s_solvers[index].used ) {
		return "";
	}
	return s_solvers[index].desc.name;
}

int PhysSolvers_GetActiveCount( const char *name ) {
	int idx = PhysSolvers_Find( name );
	if ( idx < 0 || !s_solvers[idx].desc.GetActiveCount ) {
		return 0;
	}
	return s_solvers[idx].desc.GetActiveCount();
}

static void PhysSolvers_RunPhase( physSolverPhase_t phase, float dt ) {
	int i;
	for ( i = 0; i < s_solverCount; i++ ) {
		if ( !s_solvers[i].used || !s_solvers[i].desc.enabled ) {
			continue;
		}
		if ( s_solvers[i].desc.phase != phase ) {
			continue;
		}
		if ( !s_solvers[i].desc.Step ) {
			continue;
		}
		s_solvers[i].desc.Step( dt );
	}
}

void PhysSolvers_PreStep( float dt ) {
	if ( !s_ready ) {
		return;
	}
	PhysSolvers_RunPhase( PHYS_SOLVER_PHASE_PRE_STEP, dt );
}

void PhysSolvers_PostStep( float dt ) {
	if ( !s_ready ) {
		return;
	}
	PhysSolvers_RunPhase( PHYS_SOLVER_PHASE_POST_STEP, dt );
}

void PhysSolvers_Frame( float dt ) {
	/* Prefer PhysSolvers_PreStep / PostStep around Soft Step; keep for simple callers. */
	PhysSolvers_PreStep( dt );
	PhysSolvers_PostStep( dt );
}

void PhysSolvers_DebugDraw( void ) {
	int i;
	if ( !s_ready ) {
		return;
	}
	for ( i = 0; i < s_solverCount; i++ ) {
		if ( !s_solvers[i].used || !s_solvers[i].desc.enabled ) {
			continue;
		}
		if ( s_solvers[i].desc.DebugDraw ) {
			s_solvers[i].desc.DebugDraw();
		}
	}
}

static int softstep_marker_count( void ) {
	return Phys_GetBodyCount();
}

void PhysSolvers_RegisterBuiltins( void ) {
	physSolverDesc_t d;

	/* --- PRE_STEP: force / kinematic layers that Soft Step must see --- */
	Com_Memset( &d, 0, sizeof( d ) );
	Q_strncpyz( d.name, "shadows", sizeof( d.name ) );
	d.phase = PHYS_SOLVER_PHASE_PRE_STEP;
	d.enabled = qtrue;
	d.Step = PhysProp_Frame;
	d.GetActiveCount = PhysProp_GetShadowCount;
	PhysSolvers_Register( &d );

	Com_Memset( &d, 0, sizeof( d ) );
	Q_strncpyz( d.name, "volumes", sizeof( d.name ) );
	d.phase = PHYS_SOLVER_PHASE_PRE_STEP;
	d.enabled = qtrue;
	d.Step = PhysVolume_Frame;
	d.GetActiveCount = PhysVolume_GetActiveCount;
	PhysSolvers_Register( &d );

	Com_Memset( &d, 0, sizeof( d ) );
	Q_strncpyz( d.name, "procanim", sizeof( d.name ) );
	d.phase = PHYS_SOLVER_PHASE_PRE_STEP;
	d.enabled = qtrue;
	d.Step = ProcAnim_UpdateAll;
	d.GetActiveCount = ProcAnim_GetActiveCount;
	PhysSolvers_Register( &d );

	Com_Memset( &d, 0, sizeof( d ) );
	Q_strncpyz( d.name, "motors", sizeof( d.name ) );
	d.phase = PHYS_SOLVER_PHASE_PRE_STEP;
	d.enabled = qtrue;
	d.Step = PhysMotor_UpdateAll;
	d.GetActiveCount = PhysMotor_GetActiveCount;
	PhysSolvers_Register( &d );

	/* --- Soft Step primary (marker only; integrator is Phys_StepSimulation_Impl) --- */
	Com_Memset( &d, 0, sizeof( d ) );
	Q_strncpyz( d.name, "softstep", sizeof( d.name ) );
	d.phase = PHYS_SOLVER_PHASE_POST_STEP;
	d.enabled = qtrue;
	d.Step = NULL;
	d.GetActiveCount = softstep_marker_count;
	PhysSolvers_Register( &d );

	/* --- POST_STEP companions that collide via Phys_* --- */
	Com_Memset( &d, 0, sizeof( d ) );
	Q_strncpyz( d.name, "xpbd_cloth", sizeof( d.name ) );
	d.phase = PHYS_SOLVER_PHASE_POST_STEP;
	d.enabled = qtrue;
	d.Init = Cloth_Init;
	d.Shutdown = Cloth_Shutdown;
	d.Step = Cloth_SimulateAll;
	d.DebugDraw = Cloth_DebugDrawAll;
	d.GetActiveCount = Cloth_GetActiveCount;
	PhysSolvers_Register( &d );

	Com_Memset( &d, 0, sizeof( d ) );
	Q_strncpyz( d.name, "particles", sizeof( d.name ) );
	d.phase = PHYS_SOLVER_PHASE_POST_STEP;
	d.enabled = qtrue;
	d.Init = PhysParticles_Init;
	d.Shutdown = PhysParticles_Shutdown;
	d.Step = PhysParticles_Step;
	d.DebugDraw = PhysParticles_DebugDraw;
	d.GetActiveCount = PhysParticles_GetActiveCount;
	PhysSolvers_Register( &d );

	Com_Memset( &d, 0, sizeof( d ) );
	Q_strncpyz( d.name, "softblob", sizeof( d.name ) );
	d.phase = PHYS_SOLVER_PHASE_POST_STEP;
	d.enabled = qtrue;
	d.Init = SoftBlob_Init;
	d.Shutdown = SoftBlob_Shutdown;
	d.Step = SoftBlob_Step;
	d.DebugDraw = SoftBlob_DebugDraw;
	d.GetActiveCount = SoftBlob_GetActiveCount;
	PhysSolvers_Register( &d );

	Com_Memset( &d, 0, sizeof( d ) );
	Q_strncpyz( d.name, "fluid", sizeof( d.name ) );
	d.phase = PHYS_SOLVER_PHASE_POST_STEP;
	d.enabled = qtrue;
	d.Init = PhysFluid_Init;
	d.Shutdown = PhysFluid_Shutdown;
	d.Step = PhysFluid_Step;
	d.DebugDraw = PhysFluid_DebugDraw;
	d.GetActiveCount = PhysFluid_GetActiveCount;
	PhysSolvers_Register( &d );
}
