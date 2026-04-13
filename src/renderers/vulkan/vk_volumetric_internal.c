/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

MSAA depth resolve for volumetrics, fluid simulation dispatch, perf queries.
Split from vk.c.
===========================================================================
*/

#include "tr_local.h"
#include "vk_image_layout.h"
#include "vk_volumetric_internal.h"
#include "vk_volumetric_params.h"

void vk_resolve_volumetric_depth_msaa( void )
{
	if ( !vk.msaaActive || vk.volumetric_depth_resolve_pipeline == VK_NULL_HANDLE ||
		vk.volumetric_depth_resolve_descriptor == VK_NULL_HANDLE ||
		vk.depth_image == VK_NULL_HANDLE || vk.volumetric_depth_image == VK_NULL_HANDLE )
	{
		return;
	}

	record_image_layout_transition( vk.cmd->command_buffer, vk.volumetric_depth_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.volumetric_depth_resolve_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
		vk.volumetric_depth_resolve_pipeline_layout, 0, 1, &vk.volumetric_depth_resolve_descriptor, 0, NULL );
	qvkCmdDispatch( vk.cmd->command_buffer, ( glConfig.vidWidth + 7 ) / 8, ( glConfig.vidHeight + 7 ) / 8, 1 );

	record_image_layout_transition( vk.cmd->command_buffer, vk.volumetric_depth_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
}

void vk_set_volumetric_pass_params( float stage, float x, float y, float z )
{
	if ( !vk.volumetric_params_ptr ) {
		return;
	}

	volumetric_params_t *params = (volumetric_params_t *)vk.volumetric_params_ptr;
	params->passParams[0] = stage;
	params->passParams[1] = x;
	params->passParams[2] = y;
	params->passParams[3] = z;
}

void vk_write_volumetric_timestamp( uint32_t query_index, VkPipelineStageFlagBits stage )
{
	if ( vk.volumetric_query_pool == VK_NULL_HANDLE || !qvkCmdWriteTimestamp ) {
		return;
	}
	if ( !r_volumetricFogPerfTimers || !r_volumetricFogPerfTimers->integer ) {
		return;
	}
	if ( query_index >= VK_VOLUMETRY_QUERY_USED ) {
		return;
	}
	const uint32_t query_base = vk.cmd_index * VK_VOLUMETRIC_QUERY_SLOTS;
	qvkCmdWriteTimestamp( vk.cmd->command_buffer, stage, vk.volumetric_query_pool, query_base + query_index );
}

static void vk_update_fluid_auto_scale( void )
{
	float min_resolution;
	float adjust_rate;
	int min_iterations;
	int base_iterations;
	float target_ms;
	qboolean autoscale_enabled;

	base_iterations = ( r_fogFluidPressureIterations ) ? r_fogFluidPressureIterations->integer : 12;
	if ( base_iterations < 1 ) {
		base_iterations = 1;
	} else if ( base_iterations > VK_FLUID_MAX_PRESSURE_ITERATIONS ) {
		base_iterations = VK_FLUID_MAX_PRESSURE_ITERATIONS;
	}
	min_iterations = ( r_fogFluidAutoScaleMinIterations ) ? r_fogFluidAutoScaleMinIterations->integer : 6;
	if ( min_iterations < 1 ) {
		min_iterations = 1;
	} else if ( min_iterations > base_iterations ) {
		min_iterations = base_iterations;
	}

	min_resolution = ( r_fogFluidAutoScaleMinResolution ) ? r_fogFluidAutoScaleMinResolution->value : 0.45f;
	if ( min_resolution < 0.125f ) {
		min_resolution = 0.125f;
	} else if ( min_resolution > 1.0f ) {
		min_resolution = 1.0f;
	}
	adjust_rate = ( r_fogFluidAutoScaleRate ) ? r_fogFluidAutoScaleRate->value : 0.08f;
	if ( adjust_rate < 0.01f ) {
		adjust_rate = 0.01f;
	} else if ( adjust_rate > 1.0f ) {
		adjust_rate = 1.0f;
	}
	target_ms = ( r_fogFluidTargetMs ) ? r_fogFluidTargetMs->value : 1.2f;
	if ( target_ms < 0.1f ) {
		target_ms = 0.1f;
	} else if ( target_ms > 8.0f ) {
		target_ms = 8.0f;
	}

	autoscale_enabled = ( r_fogFluidAutoScale && r_fogFluidAutoScale->integer &&
		r_fogFluid && r_fogFluid->integer && vk.volumetric_fluid_ms > 0.0f ) ? qtrue : qfalse;

	if ( vk.fluid_dynamic_resolution_scale <= 0.0f || vk.fluid_dynamic_resolution_scale > 1.0f ) {
		vk.fluid_dynamic_resolution_scale = 1.0f;
	}
	if ( vk.fluid_dynamic_pressure_iterations <= 0 ) {
		vk.fluid_dynamic_pressure_iterations = base_iterations;
	}

	if ( !autoscale_enabled ) {
		vk.fluid_dynamic_resolution_scale = 1.0f;
		vk.fluid_dynamic_pressure_iterations = base_iterations;
		return;
	}

	if ( vk.volumetric_fluid_ms > target_ms * 1.05f ) {
		vk.fluid_dynamic_resolution_scale -= adjust_rate;
		if ( vk.fluid_dynamic_resolution_scale < min_resolution ) {
			vk.fluid_dynamic_resolution_scale = min_resolution;
			if ( vk.fluid_dynamic_pressure_iterations > min_iterations ) {
				vk.fluid_dynamic_pressure_iterations--;
			}
		}
	} else if ( vk.volumetric_fluid_ms < target_ms * 0.75f ) {
		if ( vk.fluid_dynamic_pressure_iterations < base_iterations ) {
			vk.fluid_dynamic_pressure_iterations++;
		} else {
			vk.fluid_dynamic_resolution_scale += adjust_rate;
		}
	}

	if ( vk.fluid_dynamic_resolution_scale < min_resolution ) {
		vk.fluid_dynamic_resolution_scale = min_resolution;
	} else if ( vk.fluid_dynamic_resolution_scale > 1.0f ) {
		vk.fluid_dynamic_resolution_scale = 1.0f;
	}
	if ( vk.fluid_dynamic_pressure_iterations < min_iterations ) {
		vk.fluid_dynamic_pressure_iterations = min_iterations;
	} else if ( vk.fluid_dynamic_pressure_iterations > base_iterations ) {
		vk.fluid_dynamic_pressure_iterations = base_iterations;
	}
}

void vk_update_volumetric_perf_queries( void )
{
	uint64_t query_values[ VK_VOLUMETRIC_QUERY_SLOTS ];
	const uint32_t read_slot = ( vk.cmd_index + NUM_COMMAND_BUFFERS - 1 ) % NUM_COMMAND_BUFFERS;
	const uint32_t query_base = read_slot * VK_VOLUMETRIC_QUERY_SLOTS;
	const VkResult query_result = qvkGetQueryPoolResults( vk.device,
		vk.volumetric_query_pool,
		query_base,
		VK_VOLUMETRY_QUERY_USED,
		sizeof( query_values ),
		query_values,
		sizeof( uint64_t ),
		VK_QUERY_RESULT_64_BIT );
	if ( query_result != VK_SUCCESS ) {
		return;
	}

	const double to_ms = (double)vk.volumetric_timestamp_period_ns * 1e-6;
	for ( int i = 0; i < VK_VOLUMETRY_QUERY_USED - 1; i++ ) {
		const uint64_t t0 = query_values[i];
		const uint64_t t1 = query_values[i + 1];
		vk.volumetric_stage_ms[i] = ( t1 >= t0 ) ? (float)( (double)( t1 - t0 ) * to_ms ) : 0.0f;
	}
	vk.volumetric_fluid_ms = vk.volumetric_stage_ms[ VK_VOLUMETRY_QUERY_AFTER_FLUID_SIM - VK_VOLUMETRY_QUERY_FOG_START ];
	vk.volumetric_total_ms = ( query_values[ VK_VOLUMETRY_QUERY_FOG_END ] >= query_values[ VK_VOLUMETRY_QUERY_FOG_START ] ) ?
		(float)( (double)( query_values[ VK_VOLUMETRY_QUERY_FOG_END ] - query_values[ VK_VOLUMETRY_QUERY_FOG_START ] ) * to_ms ) : 0.0f;

	vk_update_fluid_auto_scale();

	if ( r_volumetricFogPerfTimers && r_volumetricFogPerfTimers->integer ) {
		int print_interval = ( r_volumetricFogPerfPrintInterval ) ? r_volumetricFogPerfPrintInterval->integer : 120;
		if ( print_interval < 1 ) {
			print_interval = 1;
		}
		if ( ( vk.volumetric_frame % (uint32_t)print_interval ) == 0u ) {
			ri.Printf( PRINT_DEVELOPER,
				"[VK][fog][perf] total=%.3fms fluid=%.3fms clear=%.3f global=%.3f volume=%.3f fluidInject=%.3f sun=%.3f local=%.3f temporal=%.3f scale=%.3f iters=%d\n",
				vk.volumetric_total_ms,
				vk.volumetric_fluid_ms,
				vk.volumetric_stage_ms[ VK_VOLUMETRY_QUERY_AFTER_CLEAR - VK_VOLUMETRY_QUERY_FOG_START ],
				vk.volumetric_stage_ms[ VK_VOLUMETRY_QUERY_AFTER_GLOBAL_DENSITY - VK_VOLUMETRY_QUERY_FOG_START ],
				vk.volumetric_stage_ms[ VK_VOLUMETRY_QUERY_AFTER_VOLUME_DENSITY - VK_VOLUMETRY_QUERY_FOG_START ],
				vk.volumetric_stage_ms[ VK_VOLUMETRY_QUERY_AFTER_FLUID_DENSITY - VK_VOLUMETRY_QUERY_FOG_START ],
				vk.volumetric_stage_ms[ VK_VOLUMETRY_QUERY_AFTER_SUN - VK_VOLUMETRY_QUERY_FOG_START ],
				vk.volumetric_stage_ms[ VK_VOLUMETRY_QUERY_AFTER_LOCAL - VK_VOLUMETRY_QUERY_FOG_START ],
				vk.volumetric_stage_ms[ VK_VOLUMETRY_QUERY_AFTER_TEMPORAL - VK_VOLUMETRY_QUERY_FOG_START ],
				vk.fluid_dynamic_resolution_scale,
				vk.fluid_dynamic_pressure_iterations );
		}
	}
}

void vk_volumetric_stage_barrier( VkImage image )
{
	if ( image == VK_NULL_HANDLE ) {
		return;
	}
	record_image_layout_transition( vk.cmd->command_buffer, image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
}

void vk_fluid_simulation_pass( float delta_time )
{
	enum {
		VK_FLUID_OP_ADVECT_VELOCITY = 0,
		VK_FLUID_OP_ADVECT_DENSITY = 1,
		VK_FLUID_OP_DIVERGENCE = 2,
		VK_FLUID_OP_PRESSURE = 3,
		VK_FLUID_OP_GRADIENT = 4
	};

	if ( !r_fogFluid || !r_fogFluid->integer ) {
		return;
	}
	if ( vk.volumetric_fluid_pipeline_layout == VK_NULL_HANDLE || vk.volumetric_fluid_descriptor == VK_NULL_HANDLE ) {
		return;
	}
	if ( vk.volumetric_fluid_advect_pipeline == VK_NULL_HANDLE ||
		vk.volumetric_fluid_divergence_pipeline == VK_NULL_HANDLE ||
		vk.volumetric_fluid_pressure_pipeline == VK_NULL_HANDLE ||
		vk.volumetric_fluid_gradient_pipeline == VK_NULL_HANDLE ) {
		return;
	}
	if ( vk.fluid_width == 0 || vk.fluid_height == 0 ) {
		return;
	}
	if ( vk.volumetric_params_ptr == NULL ) {
		return;
	}
	if ( vk.fluid_velocity_images[0] == VK_NULL_HANDLE || vk.fluid_velocity_images[1] == VK_NULL_HANDLE ||
	     vk.fluid_density_images[0] == VK_NULL_HANDLE || vk.fluid_density_images[1] == VK_NULL_HANDLE ||
	     vk.fluid_pressure_images[0] == VK_NULL_HANDLE || vk.fluid_pressure_images[1] == VK_NULL_HANDLE ||
	     vk.fluid_divergence_image == VK_NULL_HANDLE ) {
		return;
	}

	volumetric_params_t *params_rw = (volumetric_params_t *)vk.volumetric_params_ptr;
	if ( params_rw->fluidParams1[3] < 0.5f ) {
		return;
	}

	uint32_t active_width = (uint32_t)MAX( 1, (int)( params_rw->fluidParams2[0] + 0.5f ) );
	uint32_t active_height = (uint32_t)MAX( 1, (int)( params_rw->fluidParams2[1] + 0.5f ) );
	if ( active_width > vk.fluid_width ) {
		active_width = vk.fluid_width;
	}
	if ( active_height > vk.fluid_height ) {
		active_height = vk.fluid_height;
	}
	if ( active_width == 0 || active_height == 0 ) {
		return;
	}
	vk.fluid_active_width = active_width;
	vk.fluid_active_height = active_height;
	const uint32_t groups_x = ( active_width + 15 ) / 16;
	const uint32_t groups_y = ( active_height + 15 ) / 16;
	uint32_t vel_read = vk.fluid_velocity_index & 1u;
	uint32_t vel_write = vel_read ^ 1u;
	uint32_t den_read = vk.fluid_density_index & 1u;
	uint32_t den_write = den_read ^ 1u;
	uint32_t pressure_read = vk.fluid_pressure_index & 1u;
	uint32_t pressure_write = pressure_read ^ 1u;
	int pressure_iterations = (int)params_rw->fluidParams1[2];

	if ( pressure_iterations < 1 ) {
		pressure_iterations = 1;
	} else if ( pressure_iterations > VK_FLUID_MAX_PRESSURE_ITERATIONS ) {
		pressure_iterations = VK_FLUID_MAX_PRESSURE_ITERATIONS;
	}

	if ( delta_time <= 0.0f ) {
		delta_time = 1.0f / 60.0f;
	}

	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
		vk.volumetric_fluid_pipeline_layout, 0, 1, &vk.volumetric_fluid_descriptor, 0, NULL );

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.volumetric_fluid_advect_pipeline );
	vk_set_volumetric_pass_params( (float)VK_FLUID_OP_ADVECT_VELOCITY, (float)vel_read, (float)vel_write, 0.0f );
	qvkCmdDispatch( vk.cmd->command_buffer, groups_x, groups_y, 1 );
	vk_volumetric_stage_barrier( vk.fluid_velocity_images[vel_write] );

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.volumetric_fluid_divergence_pipeline );
	vk_set_volumetric_pass_params( (float)VK_FLUID_OP_DIVERGENCE, (float)vel_write, 0.0f, 0.0f );
	qvkCmdDispatch( vk.cmd->command_buffer, groups_x, groups_y, 1 );
	vk_volumetric_stage_barrier( vk.fluid_divergence_image );

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.volumetric_fluid_pressure_pipeline );
	for ( int i = 0; i < pressure_iterations; i++ ) {
		vk_set_volumetric_pass_params( (float)VK_FLUID_OP_PRESSURE, (float)pressure_read, (float)pressure_write, 0.0f );
		qvkCmdDispatch( vk.cmd->command_buffer, groups_x, groups_y, 1 );
		vk_volumetric_stage_barrier( vk.fluid_pressure_images[pressure_write] );

		const uint32_t tmp = pressure_read;
		pressure_read = pressure_write;
		pressure_write = tmp;
	}

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.volumetric_fluid_gradient_pipeline );
	vk_set_volumetric_pass_params( (float)VK_FLUID_OP_GRADIENT, (float)vel_write, (float)vel_read, (float)pressure_read );
	qvkCmdDispatch( vk.cmd->command_buffer, groups_x, groups_y, 1 );
	vk_volumetric_stage_barrier( vk.fluid_velocity_images[vel_read] );

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.volumetric_fluid_advect_pipeline );
	vk_set_volumetric_pass_params( (float)VK_FLUID_OP_ADVECT_DENSITY, (float)den_read, (float)den_write, (float)vel_read );
	qvkCmdDispatch( vk.cmd->command_buffer, groups_x, groups_y, 1 );
	vk_volumetric_stage_barrier( vk.fluid_density_images[den_write] );

	vk.fluid_velocity_index = vel_read;
	vk.fluid_density_index = den_write;
	vk.fluid_pressure_index = pressure_read;
	params_rw->fluidParams2[0] = (float)vk.fluid_active_width;
	params_rw->fluidParams2[1] = (float)vk.fluid_active_height;
	params_rw->fluidParams2[2] = (float)vk.fluid_velocity_index;
	params_rw->fluidParams2[3] = (float)vk.fluid_density_index;
}
