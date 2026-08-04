#include "tr_local.h"
#include "vk_hair_deferred.h"
#include "vk_forward_plus.h"

/* Paper-inspired deferred hair sidecar contract.  The actual groom importer
 * and compute raster stages remain opt-in follow-up work; keeping the gate and
 * ownership state here prevents hair from silently entering ordinary OIT. */
static cvar_t *r_hairDeferred;
static cvar_t *r_hairDeferredLod;
static cvar_t *r_hairDeferredFilter;
static vkHairDeferredContract_t s_contract;
static qboolean s_registered;

void vk_hair_deferred_init( void )
{
	if ( s_registered ) return;
	r_hairDeferred = ri.Cvar_Get( "r_hairDeferred", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_hairDeferred, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_hairDeferred,
		"Deferred software hair sidecar (groom visibility + clustered shade); experimental, vid_restart." );
	r_hairDeferredLod = ri.Cvar_Get( "r_hairDeferredLod", "1.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hairDeferredLod, "0.25", "8.0", CV_FLOAT );
	r_hairDeferredFilter = ri.Cvar_Get( "r_hairDeferredFilter", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hairDeferredFilter, "0", "1", CV_INTEGER );
	Com_Memset( &s_contract, 0, sizeof( s_contract ) );
	s_contract.visibilityBits = 64u;
	s_contract.visibilityFormat = 1u;
	s_contract.visibilityWords = 1u;
	s_contract.owner = 0u; /* 0 = inactive, 1 = hair sidecar */
	ri.Cmd_AddCommand( "hair_deferred_status", vk_hair_deferred_status_f );
	s_registered = qtrue;
}

void vk_hair_deferred_shutdown( void )
{
	if ( !s_registered ) return;
	ri.Cmd_RemoveCommand( "hair_deferred_status" );
	Com_Memset( &s_contract, 0, sizeof( s_contract ) );
	s_registered = qfalse;
}

const vkHairDeferredContract_t *vk_hair_deferred_contract( void )
{
	return &s_contract;
}

void vk_hair_deferred_status_f( void )
{
	qboolean active = r_hairDeferred && r_hairDeferred->integer &&
		vk.forward_plus.tile_buffer != VK_NULL_HANDLE &&
		vk.forward_plus.param_buffer != VK_NULL_HANDLE;
	ri.Printf( PRINT_ALL,
		"[VK][HairDeferred] enabled=%d owner=%s visibility=%ubit/packed64 lod=%.2f filter=%d "
		"clusters=%u state=%s\n",
		r_hairDeferred && r_hairDeferred->integer ? 1 : 0,
		active ? "hair_sidecar" : "none",
		s_contract.visibilityBits,
		r_hairDeferredLod ? r_hairDeferredLod->value : 1.0f,
		r_hairDeferredFilter ? r_hairDeferredFilter->integer : 1,
		vk_cluster_list_generation(),
		active ? "contract_only" : "inactive" );
}
