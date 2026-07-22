/*
===========================================================================
Indirect lighting contract — irradiance probes + debug.
Foundation Consolidation.
===========================================================================
*/

#include "tr_local.h"
#include "vk_indirect_light.h"

static cvar_t *r_indirectDebug;
static vkIrradianceProbe_t s_probes[VK_INDIRECT_LIGHT_MAX_PROBES];
static uint32_t s_probeCount;
static uint32_t s_generation = 1u;
static qboolean s_cmdsRegistered;

static void VK_IndirectLight_Status_f( void )
{
	uint32_t i;
	uint32_t dump = s_probeCount < 8u ? s_probeCount : 8u;

	ri.Printf( PRINT_ALL, "======== Indirect Light Status ========\n" );
	ri.Printf( PRINT_ALL, "r_indirectDebug=%d probes=%u generation=%u\n",
		r_indirectDebug ? r_indirectDebug->integer : 0, s_probeCount, s_generation );
	for ( i = 0; i < dump; i++ ) {
		const vkIrradianceProbe_t *p = &s_probes[i];
		ri.Printf( PRINT_ALL,
			"  probe[%u] pos=(%.1f,%.1f,%.1f) irr=(%.2f,%.2f,%.2f) r=%.1f gen=%u\n",
			i, p->position[0], p->position[1], p->position[2],
			p->irradiance[0], p->irradiance[1], p->irradiance[2],
			(double)p->radius, p->generation );
	}
}

void vk_indirect_light_register( void )
{
	r_indirectDebug = ri.Cvar_Get( "r_indirectDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_indirectDebug, "0", "6", CV_INTEGER );
	ri.Cvar_SetDescription( r_indirectDebug,
		"Indirect debug: 1 probe spheres, 2 irradiance, 3 SH, 4 cache, 5 leak, 6 atlas." );
	ri.Cvar_SetGroup( r_indirectDebug, CVG_RENDERER );

	if ( !s_cmdsRegistered ) {
		ri.Cmd_AddCommand( "indirect_light_status", VK_IndirectLight_Status_f );
		s_cmdsRegistered = qtrue;
		ri.Printf( PRINT_ALL, "[VK][indirect] indirect_light_status ready\n" );
	}
}

void vk_indirect_light_begin_frame( void )
{
}

void vk_indirect_light_set_probe( uint32_t index, const vec3_t pos, const vec3_t irradiance,
	float radius, uint32_t flags )
{
	vkIrradianceProbe_t *p;

	if ( index >= VK_INDIRECT_LIGHT_MAX_PROBES || !pos || !irradiance ) {
		return;
	}
	p = &s_probes[index];
	VectorCopy( pos, p->position );
	VectorCopy( irradiance, p->irradiance );
	p->radius = radius;
	p->flags = flags;
	p->generation = s_generation;
	if ( index >= s_probeCount ) {
		s_probeCount = index + 1u;
	}
}
