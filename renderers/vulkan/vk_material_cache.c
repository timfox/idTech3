/*
===========================================================================
Raster Ultra 1.8 — processed material cache.
===========================================================================
*/

#include "tr_local.h"
#include "vk_material_ir.h"
#include "vk_material_graph.h"
#include "vk_material_cache.h"

static cvar_t *r_materialCache;
static qboolean s_cmds;
static vkMaterialCacheEntry_t s_entries[VK_MAT_CACHE_MAX];
static vkMaterialCacheStats_t s_stats;

void vk_material_cache_register_cvars( void )
{
	if ( r_materialCache ) {
		return;
	}
	r_materialCache = ri.Cvar_Get( "r_materialCache", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_materialCache, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_materialCache,
		"Raster Ultra 1.8 processed material cache (latched).\n"
		"Versioned keys: source + graph + IR + compiler. Default on when Ultra materials overlay is used." );
	ri.Cvar_SetGroup( r_materialCache, CVG_RENDERER );
}

void vk_material_cache_init( void )
{
	vk_material_cache_register_cvars();
	Com_Memset( s_entries, 0, sizeof( s_entries ) );
	Com_Memset( &s_stats, 0, sizeof( s_stats ) );
	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "material_cache_status", vk_material_cache_status_f );
		s_cmds = qtrue;
	}
	ri.Printf( PRINT_ALL, "[VK][MaterialCache] %s compiler=%d ir=%d graph=%d\n",
		( r_materialCache && r_materialCache->integer ) ? "enabled" : "off",
		VK_MAT_CACHE_COMPILER, VK_MAT_IR_CACHE_VERSION, VK_MAT_GRAPH_VERSION );
}

void vk_material_cache_shutdown( void )
{
	vk_material_cache_invalidate_all();
}

void vk_material_cache_invalidate_all( void )
{
	Com_Memset( s_entries, 0, sizeof( s_entries ) );
	s_stats.entries = 0;
	s_stats.invalidations++;
}

static uint32_t Cache_Key( uint32_t sourceHash, uint32_t permutationKey )
{
	return sourceHash ^ ( permutationKey * 0x9e3779b9u ) ^
		( (uint32_t)VK_MAT_CACHE_COMPILER << 24 ) ^
		( (uint32_t)VK_MAT_IR_CACHE_VERSION << 16 ) ^
		( (uint32_t)VK_MAT_GRAPH_VERSION << 8 );
}

const vkMaterialCacheEntry_t *vk_material_cache_lookup( uint32_t sourceHash, uint32_t permutationKey )
{
	uint32_t key;
	int i;

	s_stats.lookups++;
	if ( !r_materialCache || !r_materialCache->integer ) {
		s_stats.misses++;
		return NULL;
	}
	key = Cache_Key( sourceHash, permutationKey );
	for ( i = 0; i < VK_MAT_CACHE_MAX; i++ ) {
		if ( s_entries[i].active && s_entries[i].key == key ) {
			if ( s_entries[i].compilerVersion != VK_MAT_CACHE_COMPILER ||
				s_entries[i].irCacheVersion != VK_MAT_IR_CACHE_VERSION ||
				s_entries[i].graphVersion != VK_MAT_GRAPH_VERSION ) {
				s_entries[i].active = qfalse;
				s_stats.invalidations++;
				s_stats.misses++;
				return NULL;
			}
			s_entries[i].hit = qtrue;
			s_stats.hits++;
			return &s_entries[i];
		}
	}
	s_stats.misses++;
	return NULL;
}

void vk_material_cache_store( uint32_t sourceHash, uint32_t permutationKey, uint32_t graphVersion )
{
	uint32_t key;
	int i;
	int slot = -1;

	if ( !r_materialCache || !r_materialCache->integer ) {
		return;
	}
	key = Cache_Key( sourceHash, permutationKey );
	for ( i = 0; i < VK_MAT_CACHE_MAX; i++ ) {
		if ( s_entries[i].active && s_entries[i].key == key ) {
			slot = i;
			break;
		}
		if ( !s_entries[i].active && slot < 0 ) {
			slot = i;
		}
	}
	if ( slot < 0 ) {
		slot = (int)( key % VK_MAT_CACHE_MAX );
		if ( s_entries[slot].active ) {
			s_stats.invalidations++;
		}
	}
	if ( !s_entries[slot].active ) {
		s_stats.entries++;
	}
	s_entries[slot].active = qtrue;
	s_entries[slot].key = key;
	s_entries[slot].permutationKey = permutationKey;
	s_entries[slot].sourceHash = sourceHash;
	s_entries[slot].graphVersion = graphVersion ? graphVersion : VK_MAT_GRAPH_VERSION;
	s_entries[slot].compilerVersion = VK_MAT_CACHE_COMPILER;
	s_entries[slot].irCacheVersion = VK_MAT_IR_CACHE_VERSION;
	s_entries[slot].hit = qfalse;
}

const vkMaterialCacheStats_t *vk_material_cache_stats( void )
{
	return &s_stats;
}

void vk_material_cache_status_f( void )
{
	float hitRate = 0.0f;

	if ( s_stats.lookups > 0 ) {
		hitRate = (float)s_stats.hits / (float)s_stats.lookups;
	}
	ri.Printf( PRINT_ALL, "=== Material Cache (Raster Ultra 1.8) ===\n" );
	ri.Printf( PRINT_ALL, "r_materialCache    : %d\n", r_materialCache ? r_materialCache->integer : 0 );
	ri.Printf( PRINT_ALL, "entries            : %u / %d\n", s_stats.entries, VK_MAT_CACHE_MAX );
	ri.Printf( PRINT_ALL, "lookups            : %u hits=%u misses=%u hitRate=%.2f\n",
		s_stats.lookups, s_stats.hits, s_stats.misses, hitRate );
	ri.Printf( PRINT_ALL, "invalidations      : %u\n", s_stats.invalidations );
	ri.Printf( PRINT_ALL, "runtimeCompiles    : %u (must stay 0)\n", s_stats.runtimeCompiles );
	ri.Printf( PRINT_ALL, "versions           : compiler=%d ir=%d graph=%d\n",
		VK_MAT_CACHE_COMPILER, VK_MAT_IR_CACHE_VERSION, VK_MAT_GRAPH_VERSION );
}
