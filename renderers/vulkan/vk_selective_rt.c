/* Selective ray-tracing admission: raster owns primary visibility. */
#include "tr_local.h"
#include "vk_selective_rt.h"

static cvar_t *r_selectiveRayTracing;
static cvar_t *r_selectiveShadowBudget;
static cvar_t *r_selectiveReflectionBudget;
static cvar_t *r_selectiveHeroObjectBudget;
static cvar_t *r_selectiveRaysPerPixelBudget;
static vkSelectiveRtBudget_t s_budget;
static qboolean s_inited;

void vk_srt_init( void )
{
	if ( s_inited ) return;
	r_selectiveRayTracing = ri.Cvar_Get( "r_selectiveRayTracing", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_selectiveRayTracing, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_selectiveRayTracing,
		"Enable selective RT sidecars only; raster remains authoritative for primary visibility." );
	r_selectiveShadowBudget = ri.Cvar_Get( "r_selectiveShadowBudget", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_selectiveShadowBudget, "0", "16", CV_INTEGER );
	r_selectiveReflectionBudget = ri.Cvar_Get( "r_selectiveReflectionBudget", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_selectiveReflectionBudget, "0", "16", CV_INTEGER );
	r_selectiveHeroObjectBudget = ri.Cvar_Get( "r_selectiveHeroObjectBudget", "8", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_selectiveHeroObjectBudget, "0", "128", CV_INTEGER );
	ri.Cvar_SetDescription( r_selectiveHeroObjectBudget,
		"Maximum hero/important objects admitted to selective RT sidecars per frame." );
	r_selectiveRaysPerPixelBudget = ri.Cvar_Get( "r_selectiveRaysPerPixelBudget", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_selectiveRaysPerPixelBudget, "0", "4", CV_INTEGER );
	Com_Memset( &s_budget, 0, sizeof( s_budget ) );
	ri.Cmd_AddCommand( "selective_rt_status", vk_srt_status_f );
	s_inited = qtrue;
}

void vk_srt_shutdown( void )
{
	if ( !s_inited ) return;
	ri.Cmd_RemoveCommand( "selective_rt_status" );
	Com_Memset( &s_budget, 0, sizeof( s_budget ) );
	s_inited = qfalse;
}

void vk_srt_frame_begin( void )
{
	if ( !s_inited ) vk_srt_init();
	s_budget.shadowBudget = (uint32_t)( r_selectiveShadowBudget ? r_selectiveShadowBudget->integer : 1 );
	s_budget.reflectionBudget = (uint32_t)( r_selectiveReflectionBudget ? r_selectiveReflectionBudget->integer : 1 );
	s_budget.heroObjectBudget = (uint32_t)( r_selectiveHeroObjectBudget ? r_selectiveHeroObjectBudget->integer : 8 );
	s_budget.raysPerPixelBudget = (uint32_t)( r_selectiveRaysPerPixelBudget ? r_selectiveRaysPerPixelBudget->integer : 1 );
	s_budget.shadowsAdmitted = 0;
	s_budget.reflectionsAdmitted = 0;
	s_budget.heroObjectsAdmitted = 0;
	s_budget.budgetRejects = 0;
}

static qboolean SRT_Admit( uint32_t *used, uint32_t limit )
{
	if ( !r_selectiveRayTracing || !r_selectiveRayTracing->integer || !used || *used >= limit ) {
		s_budget.budgetRejects++;
		return qfalse;
	}
	(*used)++;
	return qtrue;
}

qboolean vk_srt_admit_shadow( void ) { return SRT_Admit( &s_budget.shadowsAdmitted, s_budget.shadowBudget ); }
qboolean vk_srt_admit_reflection( void ) { return SRT_Admit( &s_budget.reflectionsAdmitted, s_budget.reflectionBudget ); }
qboolean vk_srt_admit_hero_object( void ) { return SRT_Admit( &s_budget.heroObjectsAdmitted, s_budget.heroObjectBudget ); }
const vkSelectiveRtBudget_t *vk_srt_budget( void ) { return &s_budget; }

void vk_srt_status_f( void )
{
	ri.Printf( PRINT_ALL, "[VK][SelectiveRT] enabled=%d rasterPrimary=yes shadow=%u/%u reflection=%u/%u hero=%u/%u raysPerPixel=%u rejects=%u\n",
		r_selectiveRayTracing && r_selectiveRayTracing->integer ? 1 : 0,
		s_budget.shadowsAdmitted, s_budget.shadowBudget,
		s_budget.reflectionsAdmitted, s_budget.reflectionBudget,
		s_budget.heroObjectsAdmitted, s_budget.heroObjectBudget,
		s_budget.raysPerPixelBudget, s_budget.budgetRejects );
}
