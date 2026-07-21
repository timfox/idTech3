#include "tr_local.h"
#include "vk.h"
#include "vk_fluidsim.h"
#include "vk_temporal.h"
#include "vk_util.h"
#include "vk_volumetric_fog_color.h"
#include "vk_volumetric_params.h"
#include "vk_vdb.h"
#include "vk_weather.h"
#include "vk_volumetric_clouds.h"
#include <math.h>

static void vk_tint_local_fog_volume_color( vec3_t io ) {
	const int colorMode = ( r_volumetricFogColorMode ) ? r_volumetricFogColorMode->integer : 0;
	vec3_t tint;
	float maxc;

	if ( colorMode == 1 && r_volumetricFogTint && vk_parse_fog_tint_string( r_volumetricFogTint->string, tint ) ) {
		VectorCopy( tint, io );
	} else if ( r_volumetricFogTint && vk_parse_fog_tint_string( r_volumetricFogTint->string, tint ) ) {
		io[0] *= tint[0];
		io[1] *= tint[1];
		io[2] *= tint[2];
	}

	if ( r_fogTint && vk_parse_fog_tint_string( r_fogTint->string, tint ) ) {
		io[0] *= tint[0];
		io[1] *= tint[1];
		io[2] *= tint[2];
	}

	maxc = MAX( io[0], MAX( io[1], io[2] ) );
	if ( maxc < 0.05f ) {
		if ( r_volumetricFogTint && vk_parse_fog_tint_string( r_volumetricFogTint->string, tint ) ) {
			VectorCopy( tint, io );
		} else if ( r_fogTint && vk_parse_fog_tint_string( r_fogTint->string, tint ) ) {
			VectorCopy( tint, io );
		}
	}
}

static float vk_compute_global_base_density( float rawDensity, int fogShowcase, qboolean hasLocalFogVolumes ) {
	float scale = 1.0f / 1024.0f;
	float density;

	if ( rawDensity <= 0.0f ) {
		return 0.0f;
	}

	switch ( fogShowcase ) {
		case 1: scale = 1.0f / 256.0f; break;
		case 2: scale = 1.0f / 128.0f; break;
		case 3: scale = 1.0f / 64.0f; break;
		default: break;
	}

	/*
	 * Map-local fog volumes already provide scene density. Keep any global haze
	 * contribution conservative in non-showcase mode to avoid blackouts.
	 */
	if ( hasLocalFogVolumes && fogShowcase <= 0 ) {
		scale *= 0.35f;
	}

	density = rawDensity * scale;
	return Com_Clamp( 0.0f, 0.0125f, density );
}

void vk_update_volumetric_params( void )
{
	if ( !vk.volumetric_params_ptr ) {
		return;
	}

	VDB_FrameUpdate();

	volumetric_params_t params;
	Com_Memset( &params, 0, sizeof( params ) );

	const float *projection = backEnd.viewParms.projectionMatrix;
	const float *view = backEnd.viewParms.world.modelViewMatrix;
	const int now_ms = backEnd.refdef.time;
	int depth_mode = r_volumetricFogDepthMode ? r_volumetricFogDepthMode->integer : 1;
	int fog_steps = r_volumetricFogSteps ? r_volumetricFogSteps->integer : 32;
	int quality = r_volumetricFogQuality ? r_volumetricFogQuality->integer : 2;
	int local_light_count = 0;
	int local_volume_count = 0;
	float linear_dlight_cos_outer;
	float linear_dlight_cos_inner;
	float temporal_weight = r_volumetricFogTemporalWeight ? r_volumetricFogTemporalWeight->value : 0.0f;
	{
		cvar_t *r_temporalFog = ri.Cvar_Get( "r_temporalFog", "1", CVAR_ARCHIVE_ND );
		if ( r_temporalFog && !r_temporalFog->integer ) {
			temporal_weight = 0.0f;
		}
	}
	float jitter_amount = r_volumetricFogJitter ? r_volumetricFogJitter->value : 0.0f;
	float sun_intensity = r_volumetricFogSunIntensity ? r_volumetricFogSunIntensity->value : 1.0f;
	/* Cloud shadows + weather sun visibility modulate directional volumetric lighting only. */
	sun_intensity *= vk_volumetric_clouds_sun_shadow_factor();
	float ambient_intensity = r_volumetricFogAmbientIntensity ? r_volumetricFogAmbientIntensity->value : 1.0f;
	float noise_scale = r_volumetricFogNoiseScale ? r_volumetricFogNoiseScale->value : 0.0125f;
	float noise_strength = r_volumetricFogNoiseStrength ? r_volumetricFogNoiseStrength->value : 0.85f;
	float noise_threshold = r_volumetricFogNoiseThreshold ? r_volumetricFogNoiseThreshold->value : 0.2f;
	float g_aniso = r_volumetricFogAniso ? r_volumetricFogAniso->value : 0.0f;
	float fog_density = r_volumetricFogDensity ? r_volumetricFogDensity->value : 0.0f;
	/* Raster Ultra 1.7 weather: scale global froxel density. */
	fog_density *= vk_weather_fog_density_scale();
	float extinction_scale = r_volumetricFogExtinctionScale ? r_volumetricFogExtinctionScale->value : 1.0f;
	float height_falloff = r_volumetricFogHeightFalloff ? r_volumetricFogHeightFalloff->value : 0.0f;
	float near_plane = ( r_znear ) ? r_znear->value : 8.0f;
	float far_plane = backEnd.viewParms.zFar;
	float z_exponent = ( r_volumetricFogZExponent ) ? r_volumetricFogZExponent->value : 1.5f;
	float max_distance = ( r_volumetricFogMaxDistance ) ? r_volumetricFogMaxDistance->value : 4096.0f;
	float reprojection_threshold = ( r_volumetricFogHistoryVelocityThreshold ) ? r_volumetricFogHistoryVelocityThreshold->value :
		( ( r_volumetricFogReprojectionThreshold ) ? r_volumetricFogReprojectionThreshold->value : 0.075f );

	vk_linear_dlight_cone_cosines( &linear_dlight_cos_outer, &linear_dlight_cos_inner );
	float firefly_clamp = ( r_volumetricFogFireflyClamp ) ? r_volumetricFogFireflyClamp->value : 8.0f;
	float transmittance_cutoff = ( r_volumetricFogTransmittanceCutoff ) ? r_volumetricFogTransmittanceCutoff->value : 0.01f;
	float wind_speed = ( r_volumetricFogWindSpeed ) ? r_volumetricFogWindSpeed->value : 1.0f;
	float fluid_dt;
	float fluid_viscosity = ( r_fogFluidViscosity ) ? r_fogFluidViscosity->value : 0.05f;
	float fluid_dissipation = ( r_fogFluidDissipation ) ? r_fogFluidDissipation->value : 0.985f;
	float fluid_force = ( r_fogFluidForceScale ) ? r_fogFluidForceScale->value : 1.0f;
	float fluid_velocity_clamp = ( r_fogFluidVelocityClamp ) ? r_fogFluidVelocityClamp->value : 96.0f;
	float fluid_effective_scale = 1.0f;
	float fluid_target_ms = ( r_fogFluidTargetMs ) ? r_fogFluidTargetMs->value : 1.2f;
	float fluid_flow_strength = ( r_fogFluidFlowFieldStrength ) ? r_fogFluidFlowFieldStrength->value : 0.35f;
	float fluid_flow_scale = ( r_fogFluidFlowFieldScale ) ? r_fogFluidFlowFieldScale->value : 0.004f;
	float temporal_stability = ( r_volumetricFogTemporalStability ) ? r_volumetricFogTemporalStability->value : 0.7f;
	float shadow_contrast = ( r_volumetricFogShadowContrast ) ? r_volumetricFogShadowContrast->value : 1.35f;
	int fog_showcase = ( r_volumetricFogShowcase ) ? r_volumetricFogShowcase->integer : 0;
	int fluid_wrap = ( r_fogFluidWrap ) ? r_fogFluidWrap->integer : 0;
	int fluid_quality = ( r_fogFluidQuality ) ? r_fogFluidQuality->integer : 2;
	int fluid_pressure_iterations = ( r_fogFluidPressureIterations ) ? r_fogFluidPressureIterations->integer : 12;
	int fluid_active_width = (int)vk.fluid_width;
	int fluid_active_height = (int)vk.fluid_height;
	qboolean fluid_autoscale_enabled = qfalse;
	qboolean fluid_enabled = ( r_fogFluid && r_fogFluid->integer && vk.fluid_width > 0 && vk.fluid_height > 0 ) ? qtrue : qfalse;
	qboolean has_map_fog_volumes = ( tr.world && tr.world->fogs && tr.world->numfogs > 1 && backEnd.doneWorldScene ) ? qtrue : qfalse;
	float delta_time = 1.0f / 60.0f;
	qboolean camera_cut = vk.temporal.sharedCameraCut;
	vec3_t fog_min = { -2048.0f, -2048.0f, -256.0f };
	vec3_t fog_max = {  2048.0f,  2048.0f, 1024.0f };
	vec3_t noise_scroll = { 0.03f, 0.01f, 0.02f };
	vec3_t wind_dir = { 1.0f, 0.0f, 0.0f };
	vec3_t motion_dir = { 1.0f, 0.0f, 0.0f };

	if ( vk_prev_volumetric_time_valid ) {
		int dt = now_ms - vk_prev_volumetric_time_ms;
		if ( dt < 0 || dt > 1000 ) {
			dt = 16;
		}
		delta_time = (float)dt * 0.001f;
	}
	fluid_dt = delta_time;
	if ( fluid_dt < 0.0f ) {
		fluid_dt = 0.0f;
	}
	if ( fluid_dt > ( 1.0f / 30.0f ) ) {
		fluid_dt = 1.0f / 30.0f;
	}
	vk_prev_volumetric_time_ms = now_ms;
	vk_prev_volumetric_time_valid = qtrue;
	vk_volumetric_noise_time += delta_time;

	if ( !vk_mat4_inverse( projection, params.invProj ) ) {
		Com_Memcpy( params.invProj, projection, sizeof( params.invProj ) );
	}
	if ( !vk_mat4_inverse( view, params.invView ) ) {
		Com_Memcpy( params.invView, view, sizeof( params.invView ) );
	}
	Com_Memcpy( params.proj, projection, sizeof( params.proj ) );
	myGlMultMatrix( view, projection, params.viewProj );
	if ( camera_cut ) {
		vk.has_prev_volumetric = qfalse;
		vk_volumetric_validation_state.camera_cut_events++;
		vk_near_static_view_frames = 0;
	}
	if ( vk_prev_matrices_valid && !camera_cut &&
		!vk_temporal_has_reason( VK_TEMPORAL_RESET_MISSING_PREV_DATA | VK_TEMPORAL_RESET_RENDERER_INIT |
			VK_TEMPORAL_RESET_SWAPCHAIN_CHANGE | VK_TEMPORAL_RESET_RENDER_SIZE_CHANGE |
			VK_TEMPORAL_RESET_WORLD_CHANGE | VK_TEMPORAL_RESET_CLIENT_STATE_CHANGE | VK_TEMPORAL_RESET_EXPLICIT_DEBUG ) ) {
		Com_Memcpy( params.prevView, vk_prev_view_matrix, sizeof( params.prevView ) );
		Com_Memcpy( params.prevViewProj, vk_prev_viewproj_matrix, sizeof( params.prevViewProj ) );
	} else {
		Com_Memcpy( params.prevView, view, sizeof( params.prevView ) );
		Com_Memcpy( params.prevViewProj, params.viewProj, sizeof( params.prevViewProj ) );
	}

	params.viewOrigin[0] = backEnd.viewParms.or.origin[0];
	params.viewOrigin[1] = backEnd.viewParms.or.origin[1];
	params.viewOrigin[2] = backEnd.viewParms.or.origin[2];
	params.viewOrigin[3] = ( r_volumetricFogBaseHeight ) ? r_volumetricFogBaseHeight->value : 0.0f;

	params.sunDirection[0] = tr.sunDirection[0];
	params.sunDirection[1] = tr.sunDirection[1];
	params.sunDirection[2] = tr.sunDirection[2];
	params.sunDirection[3] = 0.0f;

	vk_get_volumetric_fog_color( params.fogColor );
	{
		float fog_intensity = ( r_volumetricFogIntensity ) ? r_volumetricFogIntensity->value : 1.0f;
		params.fogColor[3] = ( fog_intensity > 0.001f ) ? fog_intensity : 0.001f;
		if ( r_volumetricFog && r_volumetricFog->integer && r_fogDebug && r_fogDebug->integer > 0 && params.fogColor[3] < 1.0f ) {
			params.fogColor[3] = 1.0f;
		}
		if ( fog_showcase == 1 && params.fogColor[3] < 1.5f ) {
			params.fogColor[3] = 1.35f;
		} else if ( fog_showcase == 2 && params.fogColor[3] < 2.0f ) {
			params.fogColor[3] = 1.6f;
		} else if ( fog_showcase >= 3 && params.fogColor[3] < 3.0f ) {
			params.fogColor[3] = 2.2f;
		}
	}

	if ( r_volumetricFogWorldMin && r_volumetricFogWorldMin->string ) {
		if ( sscanf( r_volumetricFogWorldMin->string, "%f %f %f", &fog_min[0], &fog_min[1], &fog_min[2] ) != 3 ) {
			ri.Printf( PRINT_WARNING, "r_volumetricFogWorldMin: expected 3 floats, using defaults\n" );
		}
	}
	if ( r_volumetricFogWorldMax && r_volumetricFogWorldMax->string ) {
		if ( sscanf( r_volumetricFogWorldMax->string, "%f %f %f", &fog_max[0], &fog_max[1], &fog_max[2] ) != 3 ) {
			ri.Printf( PRINT_WARNING, "r_volumetricFogWorldMax: expected 3 floats, using defaults\n" );
		}
	}
	if ( fog_max[0] <= fog_min[0] + 1.0f ) fog_max[0] = fog_min[0] + 4096.0f;
	if ( fog_max[1] <= fog_min[1] + 1.0f ) fog_max[1] = fog_min[1] + 4096.0f;
	if ( fog_max[2] <= fog_min[2] + 1.0f ) fog_max[2] = fog_min[2] + 1280.0f;
	if ( r_volumetricFogNoiseScroll && r_volumetricFogNoiseScroll->string ) {
		vk_parse_rgb_string( r_volumetricFogNoiseScroll->string, noise_scroll );
	}
	if ( r_volumetricFogWindDirection && r_volumetricFogWindDirection->string ) {
		vk_parse_rgb_string( r_volumetricFogWindDirection->string, wind_dir );
	}

	params.worldMin[0] = fog_min[0];
	params.worldMin[1] = fog_min[1];
	params.worldMin[2] = fog_min[2];
	params.worldMin[3] = 0.0f;
	params.worldMax[0] = fog_max[0];
	params.worldMax[1] = fog_max[1];
	params.worldMax[2] = fog_max[2];
	params.worldMax[3] = 0.0f;
	{
		const float size_x = fog_max[0] - fog_min[0];
		const float size_y = fog_max[1] - fog_min[1];
		params.fluidWorldMap[0] = fog_min[0];
		params.fluidWorldMap[1] = fog_min[1];
		params.fluidWorldMap[2] = ( fabsf( size_x ) > 1e-6f ) ? ( 1.0f / size_x ) : 0.0f;
		params.fluidWorldMap[3] = ( fabsf( size_y ) > 1e-6f ) ? ( 1.0f / size_y ) : 0.0f;
	}

	params.gridDim[0] = (float)vk.froxel_width;
	params.gridDim[1] = (float)vk.froxel_height;
	params.gridDim[2] = (float)vk.froxel_slices;
	params.gridDim[3] = (float)( ( r_fogDebug ) ? r_fogDebug->integer : 0 );
	params.phaseParams[0] = g_aniso;
	params.phaseParams[1] = sun_intensity;
	params.phaseParams[2] = ambient_intensity;
	params.phaseParams[3] = shadow_contrast;
	params.noiseParams[0] = noise_scale;
	params.noiseParams[1] = noise_threshold;
	params.noiseParams[2] = noise_strength;
	params.noiseParams[3] = fluid_flow_scale;
	params.noiseScroll[0] = 0.0f;
	params.noiseScroll[1] = 0.0f;
	params.noiseScroll[2] = 0.0f;
	params.noiseScroll[3] = 0.0f;

	if ( fog_steps < 1 ) fog_steps = 1;
	else if ( fog_steps > 256 ) fog_steps = 256;
	if ( depth_mode < 0 ) depth_mode = 0;
	else if ( depth_mode > 2 ) depth_mode = 2;
	if ( quality < 0 ) quality = 0;
	else if ( quality > 3 ) quality = 3;
	if ( g_aniso < -0.999f ) params.phaseParams[0] = -0.999f;
	else if ( g_aniso > 0.999f ) params.phaseParams[0] = 0.999f;
	if ( fog_density < 0.0f ) fog_density = 0.0f;
	if ( height_falloff < 0.0f ) height_falloff = 0.0f;
	if ( temporal_weight < 0.0f ) temporal_weight = 0.0f;
	else if ( temporal_weight > 1.0f ) temporal_weight = 1.0f;
	if ( jitter_amount < 0.0f ) jitter_amount = 0.0f;
	if ( noise_scale < 0.0f ) noise_scale = 0.0f;
	if ( noise_threshold < 0.0f ) noise_threshold = 0.0f;
	else if ( noise_threshold > 1.0f ) noise_threshold = 1.0f;
	if ( noise_strength < 0.0f ) noise_strength = 0.0f;
	else if ( noise_strength > 1.0f ) noise_strength = 1.0f;
	if ( extinction_scale < 0.05f ) extinction_scale = 0.05f;
	else if ( extinction_scale > 10.0f ) extinction_scale = 10.0f;
	if ( z_exponent < 1.0f ) z_exponent = 1.0f;
	if ( max_distance < near_plane + 1.0f ) max_distance = near_plane + 1.0f;
	if ( reprojection_threshold < 0.0f ) reprojection_threshold = 0.0f;
	if ( firefly_clamp < 0.0f ) firefly_clamp = 0.0f;
	if ( transmittance_cutoff < 0.0001f ) transmittance_cutoff = 0.0001f;
	else if ( transmittance_cutoff > 1.0f ) transmittance_cutoff = 1.0f;
	if ( wind_speed < 0.0f ) wind_speed = 0.0f;
	if ( fluid_quality < 0 ) fluid_quality = 0;
	else if ( fluid_quality > 3 ) fluid_quality = 3;
	if ( fluid_viscosity < 0.0f ) fluid_viscosity = 0.0f;
	if ( fluid_dissipation < 0.0f ) fluid_dissipation = 0.0f;
	else if ( fluid_dissipation > 1.0f ) fluid_dissipation = 1.0f;
	if ( fluid_force < 0.0f ) fluid_force = 0.0f;
	else if ( fluid_force > 8.0f ) fluid_force = 8.0f;
	if ( fluid_velocity_clamp < 1.0f ) fluid_velocity_clamp = 1.0f;
	else if ( fluid_velocity_clamp > 512.0f ) fluid_velocity_clamp = 512.0f;
	if ( fluid_target_ms < 0.1f ) fluid_target_ms = 0.1f;
	else if ( fluid_target_ms > 8.0f ) fluid_target_ms = 8.0f;
	if ( fluid_flow_strength < 0.0f ) fluid_flow_strength = 0.0f;
	else if ( fluid_flow_strength > 4.0f ) fluid_flow_strength = 4.0f;
	if ( fluid_flow_scale < 0.0001f ) fluid_flow_scale = 0.0001f;
	else if ( fluid_flow_scale > 1.0f ) fluid_flow_scale = 1.0f;
	if ( temporal_stability < 0.0f ) temporal_stability = 0.0f;
	else if ( temporal_stability > 1.0f ) temporal_stability = 1.0f;
	if ( shadow_contrast < 0.5f ) shadow_contrast = 0.5f;
	else if ( shadow_contrast > 4.0f ) shadow_contrast = 4.0f;
	if ( fog_showcase < 0 ) fog_showcase = 0;
	else if ( fog_showcase > 3 ) fog_showcase = 3;
	if ( has_map_fog_volumes && fog_showcase <= 0 && extinction_scale > 0.55f ) {
		/*
		 * Dense local fog brushes plus high extinction can black out water/sky.
		 * Cap extinction in normal gameplay mode; showcase modes can override.
		 */
		extinction_scale = 0.55f;
	}

	if ( fog_showcase > 0 ) {
		switch ( fog_showcase ) {
			case 1:
				fog_density = MAX( fog_density, 0.40f );
				height_falloff = MIN( height_falloff, 0.25f );
				sun_intensity = MAX( sun_intensity, 2.5f );
				ambient_intensity = MAX( ambient_intensity, 1.0f );
				noise_scale = MAX( noise_scale, 0.016f );
				noise_strength = MAX( noise_strength, 0.90f );
				noise_threshold = MIN( noise_threshold, 0.15f );
				transmittance_cutoff = MIN( transmittance_cutoff, 0.006f );
				break;
			case 2:
				fog_density = MAX( fog_density, 0.65f );
				height_falloff = MIN( height_falloff, 0.15f );
				sun_intensity = MAX( sun_intensity, 3.5f );
				ambient_intensity = MAX( ambient_intensity, 1.2f );
				noise_scale = MAX( noise_scale, 0.020f );
				noise_strength = MAX( noise_strength, 0.95f );
				noise_threshold = MIN( noise_threshold, 0.12f );
				fluid_enabled = ( vk.fluid_width > 0 && vk.fluid_height > 0 ) ? qtrue : fluid_enabled;
				fluid_force = MAX( fluid_force, 2.0f );
				fluid_flow_strength = MAX( fluid_flow_strength, 0.9f );
				fluid_velocity_clamp = MAX( fluid_velocity_clamp, 128.0f );
				fluid_pressure_iterations = MAX( fluid_pressure_iterations, 16 );
				fluid_dissipation = MAX( fluid_dissipation, 0.990f );
				transmittance_cutoff = MIN( transmittance_cutoff, 0.004f );
				break;
			default:
				fog_density = MAX( fog_density, 1.3f );
				height_falloff = MIN( height_falloff, 0.06f );
				sun_intensity = MAX( sun_intensity, 6.0f );
				ambient_intensity = MAX( ambient_intensity, 1.8f );
				noise_scale = MAX( noise_scale, 0.026f );
				noise_strength = 1.0f;
				noise_threshold = MIN( noise_threshold, 0.07f );
				temporal_weight = MAX( temporal_weight, 0.9f );
				fluid_enabled = ( vk.fluid_width > 0 && vk.fluid_height > 0 ) ? qtrue : fluid_enabled;
				fluid_force = MAX( fluid_force, 3.0f );
				fluid_flow_strength = MAX( fluid_flow_strength, 1.8f );
				fluid_velocity_clamp = MAX( fluid_velocity_clamp, 192.0f );
				fluid_pressure_iterations = MAX( fluid_pressure_iterations, 22 );
				fluid_dissipation = MAX( fluid_dissipation, 0.994f );
				transmittance_cutoff = MIN( transmittance_cutoff, 0.0015f );
				break;
		}
	}

	params.phaseParams[0] = g_aniso;
	params.phaseParams[1] = sun_intensity;
	params.phaseParams[2] = ambient_intensity;
	params.phaseParams[3] = shadow_contrast;
	if ( fluid_pressure_iterations < 1 ) fluid_pressure_iterations = 1;
	else if ( fluid_pressure_iterations > VK_FLUID_MAX_PRESSURE_ITERATIONS ) fluid_pressure_iterations = VK_FLUID_MAX_PRESSURE_ITERATIONS;
	switch ( fluid_quality ) {
		case 0:
			fluid_pressure_iterations = MIN( fluid_pressure_iterations, 8 );
			fluid_dissipation = MIN( fluid_dissipation, 0.970f );
			break;
		case 1:
			fluid_pressure_iterations = MIN( fluid_pressure_iterations, 12 );
			fluid_dissipation = MIN( fluid_dissipation, 0.982f );
			break;
		case 2:
			fluid_pressure_iterations = MIN( fluid_pressure_iterations, 16 );
			break;
		default:
			fluid_pressure_iterations = MIN( fluid_pressure_iterations, 20 );
			fluid_dissipation = MIN( fluid_dissipation, 0.995f );
			break;
	}

	fluid_autoscale_enabled = ( fluid_enabled && r_fogFluidAutoScale && r_fogFluidAutoScale->integer ) ? qtrue : qfalse;
	if ( fluid_autoscale_enabled ) {
		float min_resolution = ( r_fogFluidAutoScaleMinResolution ) ? r_fogFluidAutoScaleMinResolution->value : 0.45f;
		if ( min_resolution < 0.125f ) min_resolution = 0.125f;
		else if ( min_resolution > 1.0f ) min_resolution = 1.0f;
		fluid_effective_scale = vk.fluid_dynamic_resolution_scale;
		if ( fluid_effective_scale < min_resolution ) fluid_effective_scale = min_resolution;
		else if ( fluid_effective_scale > 1.0f ) fluid_effective_scale = 1.0f;
		if ( vk.fluid_dynamic_pressure_iterations > 0 ) {
			fluid_pressure_iterations = MIN( fluid_pressure_iterations, vk.fluid_dynamic_pressure_iterations );
		}
	}

	if ( fluid_enabled ) {
		fluid_active_width = MAX( 8, (int)( (float)vk.fluid_width * fluid_effective_scale + 0.5f ) );
		fluid_active_height = MAX( 8, (int)( (float)vk.fluid_height * fluid_effective_scale + 0.5f ) );
		fluid_active_width = MIN( fluid_active_width, (int)vk.fluid_width );
		fluid_active_height = MIN( fluid_active_height, (int)vk.fluid_height );
		if ( fluid_active_width <= 0 || fluid_active_height <= 0 ) {
			fluid_enabled = qfalse;
		}
	}
	vk.fluid_active_width = fluid_enabled ? (uint32_t)fluid_active_width : 0u;
	vk.fluid_active_height = fluid_enabled ? (uint32_t)fluid_active_height : 0u;

	params.fluidParams0[0] = fluid_dt;
	params.fluidParams0[1] = fluid_viscosity;
	params.fluidParams0[2] = fluid_dissipation;
	params.fluidParams0[3] = fluid_force;
	params.fluidParams1[0] = fluid_velocity_clamp;
	params.fluidParams1[1] = fluid_wrap ? 1.0f : 0.0f;
	params.fluidParams1[2] = (float)fluid_pressure_iterations;
	params.fluidParams1[3] = fluid_enabled ? 1.0f : 0.0f;
	params.fluidParams2[0] = fluid_enabled ? (float)vk.fluid_active_width : 0.0f;
	params.fluidParams2[1] = fluid_enabled ? (float)vk.fluid_active_height : 0.0f;
	params.fluidParams2[2] = (float)( vk.fluid_velocity_index & 1u );
	params.fluidParams2[3] = (float)( vk.fluid_density_index & 1u );

	{
		int emCount = FluidSim_GetEmitterCount();
		int i;
		if ( emCount > 16 ) emCount = 16;
		for ( i = 0; i < emCount; i++ ) {
			const fluidEmitter_t *em = FluidSim_GetEmitter( i );
			if ( !em ) continue;
			params.fluidEmitters[i][0] = em->position[0];
			params.fluidEmitters[i][1] = em->position[1];
			params.fluidEmitters[i][2] = em->position[2];
			params.fluidEmitters[i][3] = em->radius;
			params.fluidEmitterData[i][0] = em->density;
			params.fluidEmitterData[i][1] = em->temperature;
			params.fluidEmitterData[i][2] = em->velocity[0];
			params.fluidEmitterData[i][3] = em->velocity[1];
		}
		params.fluidEmitterCount[0] = (float)emCount;
		params.fluidEmitterCount[1] = FluidSim_GetVorticity();
		params.fluidEmitterCount[2] = FluidSim_GetBuoyancy();
		params.fluidEmitterCount[3] = 0.0f;
	}
	if ( VectorLengthSquared( wind_dir ) < 1e-6f ) {
		VectorSet( wind_dir, 1.0f, 0.0f, 0.0f );
	}
	VectorNormalize2( wind_dir, motion_dir );

	params.densityParams[0] = vk_compute_global_base_density( fog_density, fog_showcase, has_map_fog_volumes );
	params.densityParams[1] = height_falloff;
	params.densityParams[2] = jitter_amount;
	params.densityParams[3] = temporal_weight;
	params.scatterParams[0] = r_volumetricFogAlbedo ? r_volumetricFogAlbedo->value : 0.95f;
	params.scatterParams[1] = extinction_scale;
	params.scatterParams[2] = r_volumetricFogBlendDistance ? r_volumetricFogBlendDistance->value : 0.0f;
	params.scatterParams[3] = ( r_volumetricFogDenoise && r_volumetricFogDenoise->integer && r_volumetricFogDenoiseSigma ) ?
		r_volumetricFogDenoiseSigma->value : 0.0f;
	params.miscParams[0] = (float)fog_steps;
	params.miscParams[1] = (float)depth_mode;
	params.miscParams[2] = (float)vk.volumetric_frame;
	params.miscParams[3] = ( vk.has_prev_volumetric && temporal_weight > 0.0f && !camera_cut ) ? 1.0f : 0.0f;
	if ( near_plane < 0.001f ) near_plane = 0.001f;
	if ( far_plane > max_distance ) far_plane = max_distance;
	if ( far_plane <= near_plane + 1.0f ) far_plane = near_plane + 1.0f;
	params.sliceParams[0] = near_plane;
	params.sliceParams[1] = far_plane;
	params.sliceParams[2] = z_exponent;
	params.sliceParams[3] = max_distance;
	params.noiseParams[0] = noise_scale;
	params.noiseParams[1] = noise_threshold;
	params.noiseParams[2] = noise_strength;
	params.temporalParams[0] = reprojection_threshold;
	params.temporalParams[1] = 0.02f + temporal_stability * 0.08f;
	params.temporalParams[2] = firefly_clamp;
	params.temporalParams[3] = camera_cut ? 1.0f : 0.0f;
	params.qualityParams[0] = (float)quality;
	params.qualityParams[1] = (float)( r_volumetricFogSliceMode ? r_volumetricFogSliceMode->integer : 0 );
	params.qualityParams[2] = ( quality == 0 ) ? 0.65f : ( ( quality == 1 ) ? 0.8f : 0.9f );
	params.qualityParams[3] = transmittance_cutoff;
	params.windParams[0] = vk_volumetric_noise_time;
	params.windParams[1] = delta_time;
	params.windParams[2] = wind_speed;
	params.windParams[3] = fluid_flow_strength;
	params.noiseScroll[0] = noise_scroll[0] + motion_dir[0];
	params.noiseScroll[1] = noise_scroll[1] + motion_dir[1];
	params.noiseScroll[2] = noise_scroll[2] + motion_dir[2];

	memset( params.volumeTypeParams, 0, sizeof( params.volumeTypeParams ) );
	if ( tr.world && tr.world->fogs && backEnd.doneWorldScene ) {
		for ( int i = 1; i < tr.world->numfogs && local_volume_count < VK_VOLUMETRIC_MAX_VOLUMES; i++ ) {
			const fog_t *fog = &tr.world->fogs[i];
			const float extent_x = fog->bounds[1][0] - fog->bounds[0][0];
			const float extent_y = fog->bounds[1][1] - fog->bounds[0][1];
			const float extent_z = fog->bounds[1][2] - fog->bounds[0][2];
			const float local_density_cap = ( has_map_fog_volumes && fog_showcase <= 0 ) ? 0.012f : 0.03f;
			float local_density = fog_density;
			vec3_t local_color;

			if ( extent_x <= 0.001f || extent_y <= 0.001f || extent_z <= 0.001f ) continue;
			if ( fog->parms.depthForOpaque > 0.001f ) local_density *= ( 1.0f / fog->parms.depthForOpaque );
			if ( local_density < 0.0f ) local_density = 0.0f;
			if ( local_density > local_density_cap ) local_density = local_density_cap;
			VectorCopy( fog->color, local_color );
			vk_tint_local_fog_volume_color( local_color );
			{
				const float local_maxc = MAX( local_color[0], MAX( local_color[1], local_color[2] ) );
				if ( local_maxc < 0.05f ) {
					vec3_t tint;
					if ( r_volumetricFogTint && vk_parse_fog_tint_string( r_volumetricFogTint->string, tint ) ) {
						VectorCopy( tint, local_color );
					} else if ( r_fogTint && vk_parse_fog_tint_string( r_fogTint->string, tint ) ) {
						VectorCopy( tint, local_color );
					}
				}
			}

			params.volumeBoundsMin[local_volume_count][0] = fog->bounds[0][0];
			params.volumeBoundsMin[local_volume_count][1] = fog->bounds[0][1];
			params.volumeBoundsMin[local_volume_count][2] = fog->bounds[0][2];
			params.volumeBoundsMin[local_volume_count][3] = 0.0f;
			params.volumeBoundsMax[local_volume_count][0] = fog->bounds[1][0];
			params.volumeBoundsMax[local_volume_count][1] = fog->bounds[1][1];
			params.volumeBoundsMax[local_volume_count][2] = fog->bounds[1][2];
			params.volumeBoundsMax[local_volume_count][3] = 0.0f;
			params.volumeColorDensity[local_volume_count][0] = local_color[0];
			params.volumeColorDensity[local_volume_count][1] = local_color[1];
			params.volumeColorDensity[local_volume_count][2] = local_color[2];
			params.volumeColorDensity[local_volume_count][3] = local_density;
			params.volumeTypeParams[local_volume_count][0] = 0.0f;
			params.volumeTypeParams[local_volume_count][1] = 0.0f;
			params.volumeTypeParams[local_volume_count][2] = -1.0f;
			params.volumeTypeParams[local_volume_count][3] = 0.0f;
			local_volume_count++;
		}
	}

	{
		float sphereCenter[3] = { 0.0f, 0.0f, 0.0f };
		float sphereRadius, sphereDensity;
		int sphereOn = r_volumetricFogSphere ? r_volumetricFogSphere->integer : 0;
		if ( sphereOn && local_volume_count < VK_VOLUMETRIC_MAX_VOLUMES ) {
			sscanf( ri.Cvar_VariableString( "r_volumetricFogSphereCenter" ), "%f %f %f",
				&sphereCenter[0], &sphereCenter[1], &sphereCenter[2] );
			sphereRadius = r_volumetricFogSphereRadius ? r_volumetricFogSphereRadius->value : 128.0f;
			sphereDensity = r_volumetricFogSphereDensity ? r_volumetricFogSphereDensity->value : fog_density * 0.5f;
			params.volumeBoundsMin[local_volume_count][0] = sphereCenter[0];
			params.volumeBoundsMin[local_volume_count][1] = sphereCenter[1];
			params.volumeBoundsMin[local_volume_count][2] = sphereCenter[2];
			params.volumeBoundsMin[local_volume_count][3] = 0.0f;
			params.volumeBoundsMax[local_volume_count][0] = sphereRadius;
			params.volumeBoundsMax[local_volume_count][1] = 0.0f;
			params.volumeBoundsMax[local_volume_count][2] = 0.0f;
			params.volumeBoundsMax[local_volume_count][3] = 0.0f;
			params.volumeColorDensity[local_volume_count][0] = params.fogColor[0];
			params.volumeColorDensity[local_volume_count][1] = params.fogColor[1];
			params.volumeColorDensity[local_volume_count][2] = params.fogColor[2];
			params.volumeColorDensity[local_volume_count][3] = sphereDensity;
			params.volumeTypeParams[local_volume_count][0] = 1.0f;
			params.volumeTypeParams[local_volume_count][1] = sphereRadius;
			params.volumeTypeParams[local_volume_count][2] = -1.0f;
			params.volumeTypeParams[local_volume_count][3] = 0.0f;
			local_volume_count++;
		}
	}

	{
		float base[3] = { 0.0f, 0.0f, 0.0f };
		float top[3] = { 0.0f, 0.0f, 128.0f };
		float radius, density;
		int cylinderOn = r_volumetricFogCylinder ? r_volumetricFogCylinder->integer : 0;
		if ( cylinderOn && local_volume_count < VK_VOLUMETRIC_MAX_VOLUMES ) {
			sscanf( ri.Cvar_VariableString( "r_volumetricFogCylinderBase" ), "%f %f %f", &base[0], &base[1], &base[2] );
			sscanf( ri.Cvar_VariableString( "r_volumetricFogCylinderTop" ), "%f %f %f", &top[0], &top[1], &top[2] );
			radius = r_volumetricFogCylinderRadius ? r_volumetricFogCylinderRadius->value : 64.0f;
			density = r_volumetricFogCylinderDensity ? r_volumetricFogCylinderDensity->value : fog_density * 0.5f;
			params.volumeBoundsMin[local_volume_count][0] = base[0];
			params.volumeBoundsMin[local_volume_count][1] = base[1];
			params.volumeBoundsMin[local_volume_count][2] = base[2];
			params.volumeBoundsMin[local_volume_count][3] = 0.0f;
			params.volumeBoundsMax[local_volume_count][0] = top[0];
			params.volumeBoundsMax[local_volume_count][1] = top[1];
			params.volumeBoundsMax[local_volume_count][2] = top[2];
			params.volumeBoundsMax[local_volume_count][3] = 0.0f;
			params.volumeColorDensity[local_volume_count][0] = params.fogColor[0];
			params.volumeColorDensity[local_volume_count][1] = params.fogColor[1];
			params.volumeColorDensity[local_volume_count][2] = params.fogColor[2];
			params.volumeColorDensity[local_volume_count][3] = density;
			params.volumeTypeParams[local_volume_count][0] = 2.0f;
			params.volumeTypeParams[local_volume_count][1] = radius;
			params.volumeTypeParams[local_volume_count][2] = -1.0f;
			params.volumeTypeParams[local_volume_count][3] = 0.0f;
			local_volume_count++;
		}
	}

	{
		float apex[3] = { 0.0f, 0.0f, 64.0f };
		float base[3] = { 0.0f, 0.0f, 0.0f };
		float radius, density;
		int coneOn = r_volumetricFogCone ? r_volumetricFogCone->integer : 0;
		if ( coneOn && local_volume_count < VK_VOLUMETRIC_MAX_VOLUMES ) {
			sscanf( ri.Cvar_VariableString( "r_volumetricFogConeApex" ), "%f %f %f", &apex[0], &apex[1], &apex[2] );
			sscanf( ri.Cvar_VariableString( "r_volumetricFogConeBase" ), "%f %f %f", &base[0], &base[1], &base[2] );
			radius = r_volumetricFogConeRadius ? r_volumetricFogConeRadius->value : 96.0f;
			density = r_volumetricFogConeDensity ? r_volumetricFogConeDensity->value : fog_density * 0.5f;
			params.volumeBoundsMin[local_volume_count][0] = apex[0];
			params.volumeBoundsMin[local_volume_count][1] = apex[1];
			params.volumeBoundsMin[local_volume_count][2] = apex[2];
			params.volumeBoundsMin[local_volume_count][3] = 0.0f;
			params.volumeBoundsMax[local_volume_count][0] = base[0];
			params.volumeBoundsMax[local_volume_count][1] = base[1];
			params.volumeBoundsMax[local_volume_count][2] = base[2];
			params.volumeBoundsMax[local_volume_count][3] = 0.0f;
			params.volumeColorDensity[local_volume_count][0] = params.fogColor[0];
			params.volumeColorDensity[local_volume_count][1] = params.fogColor[1];
			params.volumeColorDensity[local_volume_count][2] = params.fogColor[2];
			params.volumeColorDensity[local_volume_count][3] = density;
			params.volumeTypeParams[local_volume_count][0] = 3.0f;
			params.volumeTypeParams[local_volume_count][1] = radius;
			params.volumeTypeParams[local_volume_count][2] = -1.0f;
			params.volumeTypeParams[local_volume_count][3] = 0.0f;
			local_volume_count++;
		}
	}

	for ( int i = 0; i < (int)backEnd.viewParms.num_dlights && local_light_count < VK_VOLUMETRIC_MAX_LIGHTS; i++ ) {
		const dlight_t *dl = &backEnd.viewParms.dlights[i];
		float radius = dl->radius;
		if ( radius <= 0.001f ) continue;

		params.lightPosRadius[local_light_count][0] = dl->origin[0];
		params.lightPosRadius[local_light_count][1] = dl->origin[1];
		params.lightPosRadius[local_light_count][2] = dl->origin[2];
		params.lightPosRadius[local_light_count][3] = radius;
		{
			vec3_t scaledColor;
			R_DynamicLightColor( dl, scaledColor );
			params.lightColorType[local_light_count][0] = MAX( scaledColor[0], 0.0f );
			params.lightColorType[local_light_count][1] = MAX( scaledColor[1], 0.0f );
			params.lightColorType[local_light_count][2] = MAX( scaledColor[2], 0.0f );
		}
		params.lightColorType[local_light_count][3] = dl->linear ? 1.0f : 0.0f;
		params.lightExtra[local_light_count][2] = ( r_fog_shadows && r_fog_shadows->integer ) ? ( dl->linear ? 1.0f : 2.0f ) : 0.0f;

		if ( dl->linear ) {
			vec3_t dir;
			float len;
			VectorSubtract( dl->origin2, dl->origin, dir );
			len = VectorNormalize( dir );
			if ( len <= 0.001f ) {
				VectorSet( dir, 0.0f, 0.0f, -1.0f );
				len = radius;
			}
			params.lightDirAngle[local_light_count][0] = dir[0];
			params.lightDirAngle[local_light_count][1] = dir[1];
			params.lightDirAngle[local_light_count][2] = dir[2];
			params.lightDirAngle[local_light_count][3] = linear_dlight_cos_outer;
			params.lightExtra[local_light_count][0] = linear_dlight_cos_inner;
			params.lightExtra[local_light_count][1] = len;
			params.lightExtra[local_light_count][3] = -1.0f;
		} else {
			params.lightDirAngle[local_light_count][0] = 0.0f;
			params.lightDirAngle[local_light_count][1] = 0.0f;
			params.lightDirAngle[local_light_count][2] = 0.0f;
			params.lightDirAngle[local_light_count][3] = -1.0f;
			params.lightExtra[local_light_count][0] = -1.0f;
			params.lightExtra[local_light_count][1] = radius;
			params.lightExtra[local_light_count][3] = -1.0f;
		}

		local_light_count++;
	}
	params.volumeCounts[0] = (float)local_volume_count;
	params.volumeCounts[1] = (float)local_light_count;
	params.volumeCounts[2] = vk.sun_shadow_valid ? 1.0f : 0.0f;
	{
		int compMode = r_volumetricFogCompositeMode ? r_volumetricFogCompositeMode->integer : 0;
		if ( compMode < 0 ) compMode = 0;
		if ( compMode > 2 ) compMode = 2;
		params.volumeCounts[3] = (float)compMode;
	}
	params.passParams[0] = (float)( r_volumetricFogIntegration ? r_volumetricFogIntegration->integer : 0 );
	if ( params.passParams[0] < 0.0f ) {
		params.passParams[0] = 0.0f;
	} else if ( params.passParams[0] > 3.0f ) {
		params.passParams[0] = 3.0f;
	}
	params.passParams[1] = (float)( ( vk.froxel_width + 1 ) / 2 );
	params.passParams[2] = (float)( ( vk.froxel_height + 1 ) / 2 );
	params.passParams[3] = (float)vk.froxel_slices;

	Matrix16Identity( params.sunShadowMatrix0 );
	if ( vk.sun_shadow_valid ) {
		Com_Memcpy( params.sunShadowMatrix0, vk.sun_shadow_matrix0, sizeof( params.sunShadowMatrix0 ) );
	}
	params.shadowParams0[0] = ( r_fogShadowBias ) ? r_fogShadowBias->value : 0.001f;
	params.shadowParams0[1] = ( r_fogShadowPcfRadius ) ? r_fogShadowPcfRadius->value : 1.0f;
	params.shadowParams0[2] = ( r_fog_shadows && r_fog_shadows->integer ) ? 1.0f : 0.0f;
	params.shadowParams0[3] = vk.sun_shadow_valid ? 1.0f : 0.0f;
	params.shadowMapSize0[0] = (float)vk.sun_shadow_width;
	params.shadowMapSize0[1] = (float)vk.sun_shadow_height;
	params.shadowMapSize0[2] = ( vk.sun_shadow_width > 0 ) ? ( 1.0f / (float)vk.sun_shadow_width ) : 0.0f;
	params.shadowMapSize0[3] = ( vk.sun_shadow_height > 0 ) ? ( 1.0f / (float)vk.sun_shadow_height ) : 0.0f;
	params.localSpotShadowMapSize[0] = (float)vk.local_spot_shadow_atlas_size;
	params.localSpotShadowMapSize[1] = (float)vk.local_spot_shadow_atlas_size;
	params.localSpotShadowMapSize[2] = ( vk.local_spot_shadow_atlas_size > 0 ) ? ( 1.0f / (float)vk.local_spot_shadow_atlas_size ) : 0.0f;
	params.localSpotShadowMapSize[3] = ( vk.local_spot_shadow_atlas_size > 0 ) ? ( 1.0f / (float)vk.local_spot_shadow_atlas_size ) : 0.0f;
	params.localPointShadowMapSize[0] = (float)vk.local_point_shadow_face_size;
	params.localPointShadowMapSize[1] = (float)vk.local_point_shadow_face_size;
	params.localPointShadowMapSize[2] = ( vk.local_point_shadow_face_size > 0 ) ? ( 1.0f / (float)vk.local_point_shadow_face_size ) : 0.0f;
	params.localPointShadowMapSize[3] = (float)vk.local_point_shadow_capacity;
	if ( params.shadowParams0[0] < 0.0f ) params.shadowParams0[0] = 0.0f;
	if ( params.shadowParams0[1] < 0.0f ) params.shadowParams0[1] = 0.0f;

	params.telemetryParams0[0] = (float)vk_volumetric_validation_state.telemetry_nan_or_inf;
	params.telemetryParams0[1] = (float)vk_volumetric_validation_state.telemetry_extinction_clamp_hits;
	params.telemetryParams0[2] = (float)vk_volumetric_validation_state.telemetry_temporal_rejects;
	params.telemetryParams0[3] = fluid_target_ms;
	params.telemetryParams1[0] = vk.volumetric_total_ms;
	params.telemetryParams1[1] = vk.volumetric_fluid_ms;
	params.telemetryParams1[2] = fluid_enabled ? fluid_effective_scale : 0.0f;
	params.telemetryParams1[3] = fluid_enabled ? (float)fluid_pressure_iterations : 0.0f;

	params.vdbParams[0] = 0.0f;
	params.vdbParams[1] = 0.0f;
	params.vdbParams[2] = 0.0f;
	params.vdbParams[3] = 0.0f;
	params.vdbWorldMin[0] = 0.0f;
	params.vdbWorldMin[1] = 0.0f;
	params.vdbWorldMin[2] = 0.0f;
	params.vdbWorldMin[3] = 0.0f;
	params.vdbWorldMax[0] = 1.0f;
	params.vdbWorldMax[1] = 1.0f;
	params.vdbWorldMax[2] = 1.0f;
	params.vdbWorldMax[3] = 0.0f;
	if ( r_vdbFog && r_vdbFog->integer ) {
		const vdbHandle_t vdb_h = VDB_GetBoundFogDensityHandle();
		vdbGridInfo_t vdb_info;

		if ( vdb_h >= 0 && VDB_IsOnGPU( vdb_h ) && VDB_GetInfo( vdb_h, &vdb_info ) ) {
			params.vdbParams[0] = r_vdbFogBlend ? Com_Clamp( 0.0f, 1.0f, r_vdbFogBlend->value ) : 0.5f;
			params.vdbParams[1] = 1.0f;
			params.vdbParams[2] = VDB_HasMajorantOnGPU( vdb_h ) ? 1.0f : 0.0f;
			params.vdbParams[3] = (float)( r_volumetricFogIntegration && r_volumetricFogIntegration->integer == 3 ? 1 : 0 );
			params.vdbWorldMin[0] = vdb_info.worldMin[0];
			params.vdbWorldMin[1] = vdb_info.worldMin[1];
			params.vdbWorldMin[2] = vdb_info.worldMin[2];
			params.vdbWorldMax[0] = vdb_info.worldMax[0];
			params.vdbWorldMax[1] = vdb_info.worldMax[1];
			params.vdbWorldMax[2] = vdb_info.worldMax[2];
		}
	}

	if ( r_fogDebug && r_fogDebug->integer > 0 && ( vk.volumetric_frame % 120u ) == 0u ) {
		ri.Printf( PRINT_ALL,
			"[VK][fog] params grid=(%.0f %.0f %.0f %.0f) phase=(%.3f %.3f %.3f %.0f) misc=(%.0f %.0f %.3f %.0f) density=(%.5f %.5f %.5f %.5f) fogColor=(%.3f %.3f %.3f %.3f) worldMin=(%.1f %.1f %.1f) worldMax=(%.1f %.1f %.1f)\n",
			params.gridDim[0], params.gridDim[1], params.gridDim[2], params.gridDim[3],
			params.phaseParams[0], params.phaseParams[1], params.phaseParams[2], params.phaseParams[3],
			params.miscParams[0], params.miscParams[1], params.miscParams[2], params.miscParams[3],
			params.densityParams[0], params.densityParams[1], params.densityParams[2], params.densityParams[3],
			params.fogColor[0], params.fogColor[1], params.fogColor[2], params.fogColor[3],
			params.worldMin[0], params.worldMin[1], params.worldMin[2],
			params.worldMax[0], params.worldMax[1], params.worldMax[2] );
		ri.Printf( PRINT_ALL, "[VK][fog] viewOrigin=(%.2f %.2f %.2f %.2f) sunDir=(%.3f %.3f %.3f)\n",
			params.viewOrigin[0], params.viewOrigin[1], params.viewOrigin[2], params.viewOrigin[3],
			params.sunDirection[0], params.sunDirection[1], params.sunDirection[2] );
		ri.Printf( PRINT_ALL, "[VK][fog] frame=%u hasHistory=%.0f jitter=%.3f\n",
			vk.volumetric_frame, params.miscParams[3], jitter_amount );
		ri.Printf( PRINT_ALL, "[VK][fog] slice near=%.3f far=%.1f zExp=%.3f maxDist=%.1f\n",
			params.sliceParams[0], params.sliceParams[1], params.sliceParams[2], params.sliceParams[3] );
		ri.Printf( PRINT_ALL, "[VK][fog] noise scale=%.5f threshold=%.3f strength=%.3f windDir=(%.3f %.3f %.3f) windSpeed=%.3f dt=%.4f\n",
			params.noiseParams[0], params.noiseParams[1], params.noiseParams[2],
			params.noiseScroll[0], params.noiseScroll[1], params.noiseScroll[2],
			params.windParams[2], params.windParams[1] );
		ri.Printf( PRINT_ALL, "[VK][fog] fluid enabled=%.0f grid=%.0fx%.0f dt=%.4f visc=%.4f diss=%.4f force=%.3f iters=%.0f clamp=%.2f wrap=%.0f velIdx=%.0f denIdx=%.0f\n",
			params.fluidParams1[3], params.fluidParams2[0], params.fluidParams2[1], params.fluidParams0[0],
			params.fluidParams0[1], params.fluidParams0[2], params.fluidParams0[3], params.fluidParams1[2],
			params.fluidParams1[0], params.fluidParams1[1], params.fluidParams2[2], params.fluidParams2[3] );
		ri.Printf( PRINT_ALL, "[VK][fog] showcase preset=%d\n", fog_showcase );
		ri.Printf( PRINT_ALL, "[VK][fog] reprojection threshold=%.4f depthEps=%.4f fireflyClamp=%.3f cameraCut=%.0f quality=%.0f checkerboard=%.0f transCut=%.4f volumes=%.0f lights=%.0f\n",
			params.temporalParams[0], params.temporalParams[1], params.temporalParams[2], params.temporalParams[3],
			params.qualityParams[0], params.qualityParams[1], params.qualityParams[3], params.volumeCounts[0], params.volumeCounts[1] );
		ri.Printf( PRINT_ALL, "[VK][fog] sun shadow enabled=%.0f valid=%.0f bias=%.6f pcf=%.3f size=%.0fx%.0f\n",
			params.shadowParams0[2], params.shadowParams0[3], params.shadowParams0[0], params.shadowParams0[1],
			params.shadowMapSize0[0], params.shadowMapSize0[1] );
	}

	Com_Memcpy( vk.volumetric_params_ptr, &params, sizeof( params ) );
	/* Prev matrices are committed in vk_temporal_commit_frame_state() only —
	 * do not overwrite shared prev state mid-frame (breaks TAA / MV consumers). */
	vk.volumetric_frame++;
}
