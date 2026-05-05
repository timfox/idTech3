/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Optional disk-backed VkPipelineCache (r_vk_pipelineCacheDisk): speeds up
cold starts by reusing driver-serialized pipeline entries keyed by
VkPhysicalDeviceProperties::pipelineCacheUUID.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_pipeline_cache_disk.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define VK_PCACHE_MAX_SERIALIZE ( 64 * 1024 * 1024 )

static void vk_pipeline_cache_disk_qpath( char *out, size_t outsz, const uint8_t *uuid )
{
	static const char hexd[] = "0123456789abcdef";
	int i;

	if ( outsz < 11u + (size_t)VK_UUID_SIZE * 2u + 1u )
		return;
	Q_strncpyz( out, "vk/pcache_", outsz );
	for ( i = 0; i < VK_UUID_SIZE; i++ ) {
		size_t pos = strlen( out );
		if ( pos + 3 >= outsz )
			break;
		out[pos] = hexd[ ( uuid[i] >> 4 ) & 0x0f ];
		out[pos + 1] = hexd[ uuid[i] & 0x0f ];
		out[pos + 2] = '\0';
	}
}

void vk_pipeline_cache_create( const VkPhysicalDeviceProperties *props )
{
	VkPipelineCacheCreateInfo ci;
	void *initial;
	int initial_len;
	char path[MAX_QPATH];
	VkResult r;

	Com_Memset( &ci, 0, sizeof( ci ) );
	ci.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	initial = NULL;
	initial_len = 0;
	path[0] = '\0';

	if ( r_vk_pipelineCacheDisk && r_vk_pipelineCacheDisk->integer ) {
		vk_pipeline_cache_disk_qpath( path, sizeof( path ), props->pipelineCacheUUID );
		initial_len = ri.FS_ReadFile( path, &initial );
		if ( initial_len > 0 && initial ) {
			ci.initialDataSize = (size_t)initial_len;
			ci.pInitialData = initial;
		}
	}

	r = qvkCreatePipelineCache( vk.device, &ci, NULL, &vk.pipelineCache );
	if ( initial ) {
		ri.FS_FreeFile( initial );
	}

	if ( r != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, "[VK] Pipeline cache: vkCreatePipelineCache failed (%d); retrying empty\n",
			(int)r );
		Com_Memset( &ci, 0, sizeof( ci ) );
		ci.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		VK_CHECK( qvkCreatePipelineCache( vk.device, &ci, NULL, &vk.pipelineCache ) );
		if ( r_vk_pipelineCacheDisk && r_vk_pipelineCacheDisk->integer ) {
			vk_pipeline_cache_disk_qpath( path, sizeof( path ), props->pipelineCacheUUID );
			ri.Printf( PRINT_ALL, "[VK] Pipeline cache disk: using empty cache (see %s after exit)\n", path );
		}
	} else if ( r_vk_pipelineCacheDisk && r_vk_pipelineCacheDisk->integer ) {
		vk_pipeline_cache_disk_qpath( path, sizeof( path ), props->pipelineCacheUUID );
		if ( initial_len > 0 )
			ri.Printf( PRINT_ALL, "[VK] Pipeline cache disk: warm start from %s (%d bytes)\n", path, initial_len );
		else
			ri.Printf( PRINT_ALL, "[VK] Pipeline cache disk: cold start (writes %s on shutdown or vid_restart)\n", path );
	}
}

void vk_pipeline_cache_save( void )
{
	VkPhysicalDeviceProperties props;
	size_t sz;
	void *buf;
	VkResult r;
	char path[MAX_QPATH];

	if ( !r_vk_pipelineCacheDisk || !r_vk_pipelineCacheDisk->integer )
		return;
	if ( vk.pipelineCache == VK_NULL_HANDLE || !qvkGetPipelineCacheData || !qvkGetPhysicalDeviceProperties )
		return;

	qvkGetPhysicalDeviceProperties( vk.physical_device, &props );
	vk_pipeline_cache_disk_qpath( path, sizeof( path ), props.pipelineCacheUUID );

	r = qvkGetPipelineCacheData( vk.device, vk.pipelineCache, &sz, NULL );
	if ( r != VK_SUCCESS ) {
		ri.Printf( PRINT_DEVELOPER, "[VK] Pipeline cache disk: GetPipelineCacheData(size) -> %d\n", (int)r );
		return;
	}
	if ( sz == 0 || sz > (size_t)VK_PCACHE_MAX_SERIALIZE ) {
		ri.Printf( PRINT_DEVELOPER, "[VK] Pipeline cache disk: skip save (size %zu)\n", sz );
		return;
	}
	if ( sz > (size_t)INT_MAX ) {
		ri.Printf( PRINT_WARNING, "[VK] Pipeline cache disk: cache blob too large for FS_WriteFile; not saved\n" );
		return;
	}
	buf = malloc( sz );
	if ( !buf ) {
		ri.Printf( PRINT_WARNING, "[VK] Pipeline cache disk: malloc(%zu) failed; not saved\n", sz );
		return;
	}
	r = qvkGetPipelineCacheData( vk.device, vk.pipelineCache, &sz, buf );
	if ( r != VK_SUCCESS ) {
		ri.Printf( PRINT_DEVELOPER, "[VK] Pipeline cache disk: GetPipelineCacheData(data) -> %d\n", (int)r );
		free( buf );
		return;
	}
	ri.FS_WriteFile( path, buf, (int)sz );
	free( buf );
	ri.Printf( PRINT_ALL, "[VK] Pipeline cache disk: saved %zu bytes to %s\n", sz, path );
}
