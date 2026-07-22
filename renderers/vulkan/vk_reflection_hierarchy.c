/*
===========================================================================
Reflection source hierarchy: planar → SSR → ray → probe → sky.
Foundation Consolidation.
===========================================================================
*/

#include "tr_local.h"
#include "vk_reflection_hierarchy.h"

static cvar_t *r_reflectionDebug;
static vkReflectionResult_t s_last;
static vkReflectionResult_t s_prev;
static qboolean s_cmdsRegistered;

static const char *VK_ReflectSourceName( vkReflectSource_t src )
{
	switch ( src ) {
	case VK_REFLECT_SRC_PLANAR: return "planar";
	case VK_REFLECT_SRC_SSR: return "ssr";
	case VK_REFLECT_SRC_RAY: return "ray";
	case VK_REFLECT_SRC_PROBE: return "probe";
	case VK_REFLECT_SRC_SKY: return "sky";
	default: return "none";
	}
}

static void VK_ReflectionHierarchy_Status_f( void )
{
	ri.Printf( PRINT_ALL, "======== Reflection Hierarchy ========\n" );
	ri.Printf( PRINT_ALL, "r_reflectionDebug=%d\n",
		r_reflectionDebug ? r_reflectionDebug->integer : 0 );
	ri.Printf( PRINT_ALL, "last: source=%s weight=%g note=%s\n",
		VK_ReflectSourceName( s_last.source ), (double)s_last.weight,
		s_last.note[0] ? s_last.note : "(none)" );
	ri.Printf( PRINT_ALL, "prev: source=%s weight=%g\n",
		VK_ReflectSourceName( s_prev.source ), (double)s_prev.weight );
}

void vk_reflection_hierarchy_register( void )
{
	r_reflectionDebug = ri.Cvar_Get( "r_reflectionDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_reflectionDebug, "0", "6", CV_INTEGER );
	ri.Cvar_SetDescription( r_reflectionDebug,
		"Reflection hierarchy debug: 1 owner, 2 SSR mask, 3 probe weight, 4 RT, 5 planar, 6 composite." );
	ri.Cvar_SetGroup( r_reflectionDebug, CVG_RENDERER );

	if ( !s_cmdsRegistered ) {
		ri.Cmd_AddCommand( "reflection_hierarchy_status", VK_ReflectionHierarchy_Status_f );
		s_cmdsRegistered = qtrue;
		ri.Printf( PRINT_ALL, "[VK][reflect] reflection_hierarchy_status ready\n" );
	}
}

void vk_reflection_hierarchy_begin_frame( void )
{
	s_prev = s_last;
	Com_Memset( &s_last, 0, sizeof( s_last ) );
}

void vk_reflection_hierarchy_note( vkReflectSource_t source, float weight, const char *note )
{
	s_last.source = source;
	s_last.weight = weight;
	if ( note && note[0] ) {
		Q_strncpyz( s_last.note, note, sizeof( s_last.note ) );
	}
}
