/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Vegetation wind compute: staging tess vertices and dispatch.
Extracted from vk.c for incremental modularization.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_postfx.h"

typedef struct {
	float positionFlex[4];
	float normalPhase[4];
} vegwind_vertex_t;

static vegwind_vertex_t vegwind_staging[VEGWIND_MAX_VERTS];
static int vegwind_staging_count;

void vk_vegetation_clear_staging( void )
{
	vegwind_staging_count = 0;
}

void vk_vegetation_add_from_tess( int oldVertexCount, int newVertexCount )
{
	int i, n, count;
	float flex, phase;

	if ( newVertexCount <= oldVertexCount || vegwind_staging_count >= VEGWIND_MAX_VERTS )
		return;

	n = newVertexCount - oldVertexCount;
	count = VEGWIND_MAX_VERTS - vegwind_staging_count;
	if ( n > count )
		n = count;

	for ( i = 0; i < n; i++ ) {
		int v = oldVertexCount + i;
		vegwind_vertex_t *dst = &vegwind_staging[vegwind_staging_count + i];

		dst->positionFlex[0] = tess.xyz[v][0];
		dst->positionFlex[1] = tess.xyz[v][1];
		dst->positionFlex[2] = tess.xyz[v][2];
		flex = tess.normal[v][1];
		dst->positionFlex[3] = ( flex > 0.0f ) ? Com_Clamp( 0.0f, 1.0f, flex ) : 0.5f;

		dst->normalPhase[0] = tess.normal[v][0];
		dst->normalPhase[1] = tess.normal[v][1];
		dst->normalPhase[2] = tess.normal[v][2];
		phase = ( tess.xyz[v][0] * 12.9898f + tess.xyz[v][2] * 78.233f );
		dst->normalPhase[3] = phase - floorf( phase );
	}

	vegwind_staging_count += n;
}

void vk_vegetation_wind_dispatch( void )
{
	typedef struct {
		float windDirection[4];
		float windParams[4];
		float gustParams[4];
		float timeParams[4];
		uint32_t vertexCount;
		uint32_t pad0;
		uint32_t pad1;
		uint32_t pad2;
	} vegwind_push_t;

	vegwind_push_t push;
	uint32_t groupCount;
	const uint32_t localSize = 64;

	if ( !PostFX_VegWind_IsEnabled() || vk.vegwind_pipeline == VK_NULL_HANDLE ||
		vk.vegwind_descriptor == VK_NULL_HANDLE )
	{
		return;
	}
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE )
		return;

	vk_end_render_pass();

	PostFX_VegWind_GetWindDir( &push.windDirection[0], &push.windDirection[1], &push.windDirection[2] );
	push.windDirection[3] = PostFX_VegWind_GetWindStrength();
	push.windParams[0] = PostFX_VegWind_GetPrimaryFreq();
	push.windParams[1] = PostFX_VegWind_GetPrimaryAmp();
	push.windParams[2] = PostFX_VegWind_GetDetailFreq();
	push.windParams[3] = PostFX_VegWind_GetDetailAmp();
	push.gustParams[0] = PostFX_VegWind_GetGustFreq();
	push.gustParams[1] = PostFX_VegWind_GetGustAmp();
	push.gustParams[2] = 0.0f;
	push.gustParams[3] = 0.0f;
	push.timeParams[0] = (float)ri.Milliseconds() / 1000.0f;
	push.timeParams[1] = 0.0f;
	push.timeParams[2] = 0.0f;
	push.timeParams[3] = 0.0f;
	push.vertexCount = vegwind_staging_count;
	push.pad0 = push.pad1 = push.pad2 = 0;

	if ( push.vertexCount > 0 && vk.vegwind_vertex_buffer != VK_NULL_HANDLE ) {
		void *ptr;
		VkDeviceSize uploadSize = (VkDeviceSize)push.vertexCount * VEGWIND_VERTEX_STRIDE;
		if ( qvkMapMemory( vk.device, vk.vegwind_vertex_memory, 0, uploadSize, 0, &ptr ) == VK_SUCCESS ) {
			Com_Memcpy( ptr, vegwind_staging, (size_t)uploadSize );
			qvkUnmapMemory( vk.device, vk.vegwind_vertex_memory );
		}
	}

	groupCount = ( push.vertexCount > 0 ) ? ( ( push.vertexCount + localSize - 1 ) / localSize ) : 1;

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.vegwind_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.pipeline_layout_vegwind, 0, 1, &vk.vegwind_descriptor, 0, NULL );
	qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout_vegwind, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( push ), &push );
	qvkCmdDispatch( vk.cmd->command_buffer, groupCount, 1, 1 );
}
