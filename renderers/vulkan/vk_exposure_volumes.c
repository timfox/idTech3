/*
 * Independently implemented from public behavioral specifications and
 * published graphics techniques. No proprietary third-party engine code used.
 *
 * Map-authored exposure controller volumes. Blends worldExposureSettings_t
 * into the single shared auto-exposure path (never per-pass).
 */
#include "tr_local.h"

#ifdef USE_VULKAN

#include "vk_world_presentation.h"
#include "vk_exposure_volumes.h"

#define EXPOSURE_VOLUME_MAX 16

typedef struct exposureVolume_s {
	float mins[3];
	float maxs[3];
	worldExposureSettings_t settings;
	float blendRadius;
	uint32_t priority;
	qboolean active;
} exposureVolume_t;

static exposureVolume_t s_volumes[EXPOSURE_VOLUME_MAX];
static int s_volumeCount;
static cvar_t *r_exposureVolumeDebug;
static worldExposureSettings_t s_blended;
static qboolean s_cmds;

static float VolumeWeight( const exposureVolume_t *v, const float origin[3] )
{
	float closest[3];
	float d, r;
	int i;
	if ( !v || !v->active ) {
		return 0.0f;
	}
	for ( i = 0; i < 3; i++ ) {
		if ( origin[i] < v->mins[i] ) {
			closest[i] = v->mins[i];
		} else if ( origin[i] > v->maxs[i] ) {
			closest[i] = v->maxs[i];
		} else {
			closest[i] = origin[i];
		}
	}
	d = 0.0f;
	{
		vec3_t delta;
		VectorSubtract( origin, closest, delta );
		d = VectorLength( delta );
	}
	r = v->blendRadius > 1.0f ? v->blendRadius : 64.0f;
	if ( d <= 0.0f ) {
		return 1.0f;
	}
	if ( d >= r ) {
		return 0.0f;
	}
	return 1.0f - ( d / r );
}

static void LerpSettings( worldExposureSettings_t *out, const worldExposureSettings_t *a,
	const worldExposureSettings_t *b, float t )
{
	out->minEV = a->minEV + ( b->minEV - a->minEV ) * t;
	out->maxEV = a->maxEV + ( b->maxEV - a->maxEV ) * t;
	out->compensation = a->compensation + ( b->compensation - a->compensation ) * t;
	out->brightenSpeed = a->brightenSpeed + ( b->brightenSpeed - a->brightenSpeed ) * t;
	out->darkenSpeed = a->darkenSpeed + ( b->darkenSpeed - a->darkenSpeed ) * t;
	out->lowPercentile = a->lowPercentile + ( b->lowPercentile - a->lowPercentile ) * t;
	out->highPercentile = a->highPercentile + ( b->highPercentile - a->highPercentile ) * t;
	out->skyWeight = a->skyWeight + ( b->skyWeight - a->skyWeight ) * t;
	out->centerWeight = a->centerWeight + ( b->centerWeight - a->centerWeight ) * t;
}

void vk_exposure_volumes_update( const float viewOrigin[3], float dt )
{
	worldExposureSettings_t base;
	worldExposureSettings_t accum;
	float wsum = 0.0f;
	int i;
	(void)dt;

	vk_world_exposure_settings_defaults( &base );
	Com_Memset( &accum, 0, sizeof( accum ) );

	if ( !viewOrigin || s_volumeCount <= 0 ) {
		s_blended = base;
		return;
	}

	for ( i = 0; i < s_volumeCount; i++ ) {
		float w = VolumeWeight( &s_volumes[i], viewOrigin );
		if ( w <= 0.0f ) {
			continue;
		}
		accum.minEV += s_volumes[i].settings.minEV * w;
		accum.maxEV += s_volumes[i].settings.maxEV * w;
		accum.compensation += s_volumes[i].settings.compensation * w;
		accum.brightenSpeed += s_volumes[i].settings.brightenSpeed * w;
		accum.darkenSpeed += s_volumes[i].settings.darkenSpeed * w;
		accum.lowPercentile += s_volumes[i].settings.lowPercentile * w;
		accum.highPercentile += s_volumes[i].settings.highPercentile * w;
		accum.skyWeight += s_volumes[i].settings.skyWeight * w;
		accum.centerWeight += s_volumes[i].settings.centerWeight * w;
		wsum += w;
	}

	if ( wsum <= 1e-4f ) {
		s_blended = base;
	} else {
		float inv = 1.0f / wsum;
		accum.minEV *= inv;
		accum.maxEV *= inv;
		accum.compensation *= inv;
		accum.brightenSpeed *= inv;
		accum.darkenSpeed *= inv;
		accum.lowPercentile *= inv;
		accum.highPercentile *= inv;
		accum.skyWeight *= inv;
		accum.centerWeight *= inv;
		/* Soft mix toward base when partially outside. */
		LerpSettings( &s_blended, &base, &accum, Com_Clamp( 0.0f, 1.0f, wsum ) );
	}

	vk_world_exposure_settings_apply( &s_blended );

	if ( r_exposureVolumeDebug && r_exposureVolumeDebug->integer ) {
		ri.Printf( PRINT_DEVELOPER,
			"[VK][expVol] wsum=%.3f brighten=%.2f darken=%.2f skyW=%.2f\n",
			wsum, s_blended.brightenSpeed, s_blended.darkenSpeed, s_blended.skyWeight );
	}
}

static void ExposureVolume_Status_f( void )
{
	ri.Printf( PRINT_ALL, "======== exposure_volume_status ========\n" );
	ri.Printf( PRINT_ALL, "active_volumes=%d / %d\n", s_volumeCount, EXPOSURE_VOLUME_MAX );
	ri.Printf( PRINT_ALL, "blended: brighten=%.3g darken=%.3g skyW=%.3g hiPct=%.3g comp=%.3g\n",
		s_blended.brightenSpeed, s_blended.darkenSpeed, s_blended.skyWeight,
		s_blended.highPercentile, s_blended.compensation );
	ri.Printf( PRINT_ALL, "contract: single SceneHDR exposure path (sky/world/weapon/WBOIT)\n" );
	ri.Printf( PRINT_ALL, "========================================\n" );
}

void vk_exposure_volumes_register( void )
{
	r_exposureVolumeDebug = ri.Cvar_Get( "r_exposureVolumeDebug", "0", CVAR_TEMP );
	ri.Cvar_CheckRange( r_exposureVolumeDebug, "0", "2", CV_INTEGER );
	s_volumeCount = 0;
	vk_world_exposure_settings_defaults( &s_blended );
	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "exposure_volume_status", ExposureVolume_Status_f );
		s_cmds = qtrue;
	}
}

#endif
