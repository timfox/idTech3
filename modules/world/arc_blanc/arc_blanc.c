/*
 * Arc Blanc main module — ocean frame tick, hull registry, physics hooks.
 */
#include "arc_blanc.h"
#include "arc_blanc_internal.h"
#include "../../qcommon/qcommon.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef USE_BULLET_PHYSICS
#include "../../physics/phys_bullet.h"
#endif

static cvar_t *r_arcBlanc;
static cvar_t *r_arcBlancWind;
static cvar_t *r_arcBlancFetch;
static cvar_t *r_arcBlancSwell;
static cvar_t *r_arcBlancDirectional;
static cvar_t *r_arcBlancSpread;
static cvar_t *r_arcBlancGrid;
static cvar_t *r_arcBlancTile;
static cvar_t *r_arcBlancWindDir;
static cvar_t *r_arcBlancSeaLevel;
static cvar_t *r_arcBlancAmplitude;
static cvar_t *r_arcBlancHeightScale;
static cvar_t *r_arcBlancChoppiness;
static cvar_t *r_arcBlancWaveSpeed;
static cvar_t *r_arcBlancGustStrength;
static cvar_t *r_arcBlancGustSpeed;
static cvar_t *r_arcBlancUpdateHz;
static cvar_t *r_arcBlancMaxSubsteps;
static cvar_t *r_arcBlancGpu;
static cvar_t *r_arcBlancWake;
static cvar_t *r_arcBlancGpuVelocity;

static abOceanState_t s_ocean;
static arcBlancGpuStepFn s_gpuStepFn;
static qboolean s_gpuOceanActive;
static abHull_t s_hulls[AB_MAX_HULLS];
static qboolean s_loggedEnable = qfalse;
static arcBlancHeightExport_t s_heightExport;
static float s_stepAccumulator;

const abOceanState_t *ArcBlanc_GetOceanForTest( void );

static void ab_log_enable_once( void )
{
	if ( s_loggedEnable ) {
		return;
	}
	s_loggedEnable = qtrue;
	Com_Printf( "[arc_blanc] enabled wind=%.1f fetch=%.0f grid=%d tile=%.0f\n",
		r_arcBlancWind ? r_arcBlancWind->value : 20.0f,
		r_arcBlancFetch ? r_arcBlancFetch->value : 1000.0f,
		r_arcBlancGrid ? r_arcBlancGrid->integer : AB_DEFAULT_GRID_N,
		r_arcBlancTile ? r_arcBlancTile->value : 256.0f );
}

static void ab_sync_ocean_params( void )
{
	s_ocean.windSpeed = r_arcBlancWind ? r_arcBlancWind->value : 20.0f;
	s_ocean.fetch = r_arcBlancFetch ? r_arcBlancFetch->value : 1000.0f;
	s_ocean.swell = r_arcBlancSwell ? r_arcBlancSwell->value : 0.5f;
	s_ocean.directional = r_arcBlancDirectional ? r_arcBlancDirectional->value : 1.0f;
	s_ocean.spread = r_arcBlancSpread ? r_arcBlancSpread->value : 0.0f;
	s_ocean.amplitudeScale = r_arcBlancAmplitude ? r_arcBlancAmplitude->value : 1.0f;
	s_ocean.heightScale = r_arcBlancHeightScale ? r_arcBlancHeightScale->value : 1.0f;
	s_ocean.chopScale = r_arcBlancChoppiness ? r_arcBlancChoppiness->value : 1.0f;
	s_ocean.waveSpeed = r_arcBlancWaveSpeed ? r_arcBlancWaveSpeed->value : 1.0f;
	s_ocean.gustStrength = r_arcBlancGustStrength ? r_arcBlancGustStrength->value : 0.0f;
	s_ocean.gustSpeed = r_arcBlancGustSpeed ? r_arcBlancGustSpeed->value : 0.5f;
	s_ocean.windDirRad = ( r_arcBlancWindDir ? r_arcBlancWindDir->value : 0.0f ) * (float)M_PI / 180.0f;
	if ( r_arcBlancGrid && r_arcBlancGrid->integer != s_ocean.gridN ) {
		int n = r_arcBlancGrid->integer;
		if ( n < 16 ) {
			n = 16;
		}
		if ( n > AB_MAX_GRID_N ) {
			n = AB_MAX_GRID_N;
		}
		if ( ( n & ( n - 1 ) ) != 0 ) {
			n = AB_DEFAULT_GRID_N;
		}
		s_ocean.gridN = n;
		s_ocean.spectrumDirty = qtrue;
	}
	if ( r_arcBlancTile ) {
		s_ocean.tileSize = r_arcBlancTile->value;
	}
}

static void ab_hull_sync_physics( abHull_t *hull )
{
#ifdef USE_BULLET_PHYSICS
	if ( hull && hull->active && hull->physBody > 0 ) {
		physTransform_t tr;
		vec3_t delta;
		int i;

		Phys_GetBodyTransform( hull->physBody, &tr );
		VectorSubtract( tr.position, hull->origin, delta );
		if ( VectorLength( delta ) > 1e-6f ) {
			for ( i = 0; i < hull->triangleCount; i++ ) {
				VectorAdd( hull->triangles[i].v0.v, delta, hull->triangles[i].v0.v );
				VectorAdd( hull->triangles[i].v1.v, delta, hull->triangles[i].v1.v );
				VectorAdd( hull->triangles[i].v2.v, delta, hull->triangles[i].v2.v );
			}
			VectorAdd( hull->boundsMin, delta, hull->boundsMin );
			VectorAdd( hull->boundsMax, delta, hull->boundsMax );
		}
		VectorCopy( tr.position, hull->origin );
		VectorCopy( tr.linearVelocity, hull->velocity );
	}
#else
	(void)hull;
#endif
}

static void ab_build_box_tris( abHull_t *hull, const vec3_t origin, const vec3_t mins, const vec3_t maxs )
{
	static const int faces[12][3] = {
		{ 0, 1, 2 }, { 0, 2, 3 },
		{ 4, 6, 5 }, { 4, 7, 6 },
		{ 0, 4, 5 }, { 0, 5, 1 },
		{ 1, 5, 6 }, { 1, 6, 2 },
		{ 2, 6, 7 }, { 2, 7, 3 },
		{ 3, 7, 4 }, { 3, 4, 0 }
	};
	vec3_t corners[8];
	int f, v;

	corners[0][0] = origin[0] + mins[0]; corners[0][1] = origin[1] + mins[1]; corners[0][2] = origin[2] + mins[2];
	corners[1][0] = origin[0] + maxs[0]; corners[1][1] = origin[1] + mins[1]; corners[1][2] = origin[2] + mins[2];
	corners[2][0] = origin[0] + maxs[0]; corners[2][1] = origin[1] + mins[1]; corners[2][2] = origin[2] + maxs[2];
	corners[3][0] = origin[0] + mins[0]; corners[3][1] = origin[1] + mins[1]; corners[3][2] = origin[2] + maxs[2];
	corners[4][0] = origin[0] + mins[0]; corners[4][1] = origin[1] + maxs[1]; corners[4][2] = origin[2] + mins[2];
	corners[5][0] = origin[0] + maxs[0]; corners[5][1] = origin[1] + maxs[1]; corners[5][2] = origin[2] + mins[2];
	corners[6][0] = origin[0] + maxs[0]; corners[6][1] = origin[1] + maxs[1]; corners[6][2] = origin[2] + maxs[2];
	corners[7][0] = origin[0] + mins[0]; corners[7][1] = origin[1] + maxs[1]; corners[7][2] = origin[2] + maxs[2];

	hull->triangleCount = 12;
	for ( f = 0; f < 12; f++ ) {
		abTriangle_t *tri = &hull->triangles[f];
		(void)v;
		VectorCopy( corners[faces[f][0]], tri->v0.v );
		VectorCopy( corners[faces[f][1]], tri->v1.v );
		VectorCopy( corners[faces[f][2]], tri->v2.v );
	}
	VectorCopy( origin, hull->origin );
	VectorAdd( origin, mins, hull->boundsMin );
	VectorAdd( origin, maxs, hull->boundsMax );
	hull->hullHeight = maxs[1] - mins[1];
	hull->hullVolume = ( maxs[0] - mins[0] ) * ( maxs[1] - mins[1] ) * ( maxs[2] - mins[2] );
	hull->fdmZoneSize = ( maxs[0] - mins[0] ) > ( maxs[2] - mins[2] )
		? 2.0f * ( maxs[0] - mins[0] ) : 2.0f * ( maxs[2] - mins[2] );
}

void ArcBlanc_Init( void )
{
	int i;

	r_arcBlanc = Cvar_Get( "r_arcBlanc", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( r_arcBlanc,
		"Enable Arc Blanc real-time ocean (Tessendorf FFT + solid-fluid coupling)." );
	r_arcBlancWind = Cvar_Get( "r_arcBlancWind", "20", CVAR_ARCHIVE );
	Cvar_SetDescription( r_arcBlancWind, "Wind speed (m/s) for JONSWAP spectrum." );
	r_arcBlancFetch = Cvar_Get( "r_arcBlancFetch", "1000", CVAR_ARCHIVE );
	Cvar_SetDescription( r_arcBlancFetch, "Wind fetch (m) for JONSWAP spectrum." );
	r_arcBlancSwell = Cvar_Get( "r_arcBlancSwell", "0.5", CVAR_ARCHIVE );
	Cvar_SetDescription( r_arcBlancSwell, "Swell parameter 0-1 (Horvath directional swell)." );
	r_arcBlancDirectional = Cvar_Get( "r_arcBlancDirectional", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( r_arcBlancDirectional, "Directional spectrum blend 0=neutral 1=Donelan-Banner." );
	r_arcBlancSpread = Cvar_Get( "r_arcBlancSpread", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( r_arcBlancSpread,
		"Directional wave spread shaping. Higher values narrow the wave cone around wind direction." );
	r_arcBlancGrid = Cvar_Get( "r_arcBlancGrid", "128", CVAR_ARCHIVE );
	Cvar_SetDescription( r_arcBlancGrid, "FFT grid resolution (power of two, max 256)." );
	r_arcBlancTile = Cvar_Get( "r_arcBlancTile", "256", CVAR_ARCHIVE );
	Cvar_SetDescription( r_arcBlancTile, "Ocean tile size in world units." );
	r_arcBlancWindDir = Cvar_Get( "r_arcBlancWindDir", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( r_arcBlancWindDir, "Wind direction in degrees." );
	r_arcBlancSeaLevel = Cvar_Get( "r_arcBlancSeaLevel", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( r_arcBlancSeaLevel, "Base sea level offset (world units)." );
	r_arcBlancAmplitude = Cvar_Get( "r_arcBlancAmplitude", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( r_arcBlancAmplitude,
		"Master wave amplitude scale. Affects both vertical height and horizontal displacement." );
	r_arcBlancHeightScale = Cvar_Get( "r_arcBlancHeightScale", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( r_arcBlancHeightScale,
		"Additional vertical-only wave scale layered on top of r_arcBlancAmplitude." );
	r_arcBlancChoppiness = Cvar_Get( "r_arcBlancChoppiness", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( r_arcBlancChoppiness,
		"Horizontal displacement scale. Higher values sharpen crests and make waves feel choppier." );
	r_arcBlancWaveSpeed = Cvar_Get( "r_arcBlancWaveSpeed", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( r_arcBlancWaveSpeed, "Scales how quickly the ocean simulation advances over time." );
	r_arcBlancGustStrength = Cvar_Get( "r_arcBlancGustStrength", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( r_arcBlancGustStrength,
		"Wind gust amplitude added as a time-varying multiplier to wave height and chop." );
	r_arcBlancGustSpeed = Cvar_Get( "r_arcBlancGustSpeed", "0.5", CVAR_ARCHIVE );
	Cvar_SetDescription( r_arcBlancGustSpeed, "Wind gust animation speed." );
	r_arcBlancUpdateHz = Cvar_Get( "r_arcBlancUpdateHz", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( r_arcBlancUpdateHz,
		"Fixed simulation update rate in Hz for performance/cinematic control. 0 = update every frame." );
	r_arcBlancMaxSubsteps = Cvar_Get( "r_arcBlancMaxSubsteps", "4", CVAR_ARCHIVE );
	Cvar_SetDescription( r_arcBlancMaxSubsteps,
		"Maximum fixed ocean steps to run in one rendered frame when r_arcBlancUpdateHz is enabled." );
	r_arcBlancGpu = Cvar_Get( "r_arcBlancGpu", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( r_arcBlancGpu,
		"Arc Blanc GPU FFT ocean: 0=CPU, 1=GPU compute + readback for physics." );
	r_arcBlancWake = Cvar_Get( "r_arcBlancWake", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( r_arcBlancWake,
		"Scale for solid-to-fluid interactive wake height added to ocean sampling." );
	r_arcBlancGpuVelocity = Cvar_Get( "r_arcBlancGpuVelocity", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( r_arcBlancGpuVelocity,
		"Arc Blanc GPU depth velocity slices when r_arcBlancGpu 1 (0=CPU IFFT)." );

	AB_Ocean_InitDefaults( &s_ocean, AB_DEFAULT_GRID_N, 256.0f );
	s_gpuStepFn = NULL;
	s_gpuOceanActive = qfalse;
	Com_Memset( s_hulls, 0, sizeof( s_hulls ) );
	for ( i = 0; i < AB_MAX_HULLS; i++ ) {
		s_hulls[i].triangles = (abTriangle_t *)Z_Malloc( 12 * sizeof( abTriangle_t ) );
		s_hulls[i].cdWater = 0.8f;
		s_hulls[i].cdAir = 0.05f;
		s_hulls[i].maskBw = 1.0f;
	}
	s_loggedEnable = qfalse;
	s_stepAccumulator = 0.0f;
	Com_Memset( &s_heightExport, 0, sizeof( s_heightExport ) );

	Cmd_AddCommand( "arc_blanc_status", ArcBlanc_Status_f );
	Cmd_AddCommand( "arc_blanc_reseed", ArcBlanc_Reseed_f );
	Cmd_AddCommand( "arc_blanc_sample", ArcBlanc_Sample_f );
	Cmd_AddCommand( "arc_blanc_preset", ArcBlanc_Preset_f );
}

void ArcBlanc_Shutdown( void )
{
	int i;
	Cmd_RemoveCommand( "arc_blanc_status" );
	Cmd_RemoveCommand( "arc_blanc_reseed" );
	Cmd_RemoveCommand( "arc_blanc_sample" );
	Cmd_RemoveCommand( "arc_blanc_preset" );
	for ( i = 0; i < AB_MAX_HULLS; i++ ) {
		if ( s_hulls[i].triangles ) {
			Z_Free( s_hulls[i].triangles );
			s_hulls[i].triangles = NULL;
		}
	}
	s_loggedEnable = qfalse;
	s_stepAccumulator = 0.0f;
}

qboolean ArcBlanc_Enabled( void )
{
	return r_arcBlanc && r_arcBlanc->integer != 0;
}

qboolean ArcBlanc_GpuWanted( void )
{
	return ArcBlanc_Enabled() && r_arcBlancGpu && r_arcBlancGpu->integer > 0;
}

void ArcBlanc_SetGpuStepFn( arcBlancGpuStepFn fn )
{
	s_gpuStepFn = fn;
}

void ArcBlanc_FillGpuParams( arcBlancGpuParams_t *out, byte *rgbaBuf, int rgbaMax,
	int *outW, int *outH )
{
	int c;
	float minH = 1e9f, maxH = -1e9f;
	int n2;

	if ( !out ) {
		return;
	}

	Com_Memset( out, 0, sizeof( *out ) );
	out->gridN = s_ocean.gridN;
	out->cascadeCount = AB_CASCADE_COUNT;
	out->time = s_ocean.time;
	for ( c = 0; c < AB_CASCADE_COUNT; c++ ) {
		out->tileLength[c] = s_ocean.cascades[c].length;
		out->h0[c] = (const arcBlancComplex_t *)s_ocean.fields[c].spec.h0;
		out->h0conj[c] = (const arcBlancComplex_t *)s_ocean.fields[c].spec.h0conj;
		out->omega[c] = s_ocean.fields[c].spec.omega;
		out->kMag[c] = s_ocean.fields[c].spec.kMag;
	}

	n2 = s_ocean.gridN * s_ocean.gridN;
	out->outCombinedHeight = s_ocean.combinedHeight;
	out->outCombinedDispX = s_ocean.combinedDispX;
	out->outCombinedDispZ = s_ocean.combinedDispZ;
	out->outRgba = rgbaBuf;
	out->rgbaMaxBytes = rgbaMax;
	out->outWidth = outW;
	out->outHeight = outH;

	if ( s_heightExport.minHeight < s_heightExport.maxHeight ) {
		minH = s_heightExport.minHeight;
		maxH = s_heightExport.maxHeight;
	} else {
		int j;
		for ( j = 0; j < n2; j++ ) {
			if ( s_ocean.combinedHeight[j] < minH ) {
				minH = s_ocean.combinedHeight[j];
			}
			if ( s_ocean.combinedHeight[j] > maxH ) {
				maxH = s_ocean.combinedHeight[j];
			}
		}
	}
	out->minHeight = minH;
	out->maxHeight = maxH;

	AB_Ocean_FillDepthSamples( out->depthSamples, ARC_BLANC_VELOCITY_SAMPLES );
	for ( c = 0; c < ARC_BLANC_VELOCITY_SAMPLES; c++ ) {
		out->outVelocitySlice[c] = s_ocean.velocitySlices[c];
	}
	out->updateVelocityGpu = ( r_arcBlancGpuVelocity && r_arcBlancGpuVelocity->integer > 0 ) ? qtrue : qfalse;
}

static void ab_update_height_export( void )
{
	int n2 = s_ocean.gridN * s_ocean.gridN;
	int j;
	float minH = 1e9f, maxH = -1e9f;

	for ( j = 0; j < n2; j++ ) {
		if ( s_ocean.combinedHeight[j] < minH ) {
			minH = s_ocean.combinedHeight[j];
		}
		if ( s_ocean.combinedHeight[j] > maxH ) {
			maxH = s_ocean.combinedHeight[j];
		}
	}

	s_heightExport.heights = s_ocean.combinedHeight;
	s_heightExport.gridN = s_ocean.gridN;
	s_heightExport.tileSize = s_ocean.tileSize;
	VectorClear( s_heightExport.origin );
	s_heightExport.minHeight = minH;
	s_heightExport.maxHeight = maxH;
}

static void ab_simulate_step( float dt )
{
	int i;
	arcBlancGpuParams_t gpuParams;
	qboolean gpuOk = qfalse;

	if ( s_ocean.spectrumDirty ) {
		AB_Ocean_UpdateSpectrum( &s_ocean );
	}

	if ( ArcBlanc_GpuWanted() && s_gpuStepFn ) {
		s_ocean.time += dt;
		ArcBlanc_FillGpuParams( &gpuParams, NULL, 0, NULL, NULL );
		gpuOk = s_gpuStepFn( &gpuParams );
		if ( gpuOk ) {
			if ( !gpuParams.updateVelocityGpu ) {
				AB_Ocean_UpdateVelocitySlices( &s_ocean );
			}
			s_gpuOceanActive = qtrue;
		} else {
			s_gpuOceanActive = qfalse;
			s_ocean.time -= dt;
			AB_Ocean_UpdateTime( &s_ocean, dt );
		}
	} else {
		s_gpuOceanActive = qfalse;
		AB_Ocean_UpdateTime( &s_ocean, dt );
	}

	for ( i = 0; i < AB_MAX_HULLS; i++ ) {
		abHull_t *hull = &s_hulls[i];
		if ( !hull->active ) {
			continue;
		}
		ab_hull_sync_physics( hull );
		AB_Coupling_UpdateHullGeometry( hull, &s_ocean );
		AB_Coupling_ComputeForces( hull, &s_ocean );
		AB_Coupling_BuildMask( hull, &s_ocean );
		AB_Coupling_StepFDM( hull, &s_ocean, dt );

#ifdef USE_BULLET_PHYSICS
		if ( hull->physBody > 0 ) {
			Phys_ApplyForce( hull->physBody, hull->forceBuoyancy, hull->immersionCenter );
			Phys_ApplyForce( hull->physBody, hull->forceWater, hull->immersionCenter );
			Phys_ApplyForce( hull->physBody, hull->forceAir, hull->origin );
		}
#endif
	}

	{
		const float wakeScale = r_arcBlancWake ? r_arcBlancWake->value : 1.0f;
		if ( !s_gpuOceanActive ) {
			AB_Ocean_CombineCascades( &s_ocean );
		}
		AB_Coupling_ApplyWakesToHeightGrid( &s_ocean, s_hulls, AB_MAX_HULLS, wakeScale );
	}

	ab_update_height_export();
}

void ArcBlanc_Frame( float dt )
{
	int maxSubsteps;
	float updateHz;
	float fixedStep;
	int steps;

	if ( !ArcBlanc_Enabled() ) {
		s_gpuOceanActive = qfalse;
		s_stepAccumulator = 0.0f;
		return;
	}
	ab_log_enable_once();
	ab_sync_ocean_params();

	updateHz = r_arcBlancUpdateHz ? r_arcBlancUpdateHz->value : 0.0f;
	maxSubsteps = r_arcBlancMaxSubsteps ? r_arcBlancMaxSubsteps->integer : 4;
	if ( maxSubsteps < 1 ) {
		maxSubsteps = 1;
	}

	if ( updateHz <= 0.0f ) {
		ab_simulate_step( dt );
		return;
	}

	if ( updateHz > 240.0f ) {
		updateHz = 240.0f;
	}
	fixedStep = 1.0f / updateHz;
	s_stepAccumulator += dt;
	if ( s_stepAccumulator > fixedStep * (float)maxSubsteps ) {
		s_stepAccumulator = fixedStep * (float)maxSubsteps;
	}

	steps = 0;
	while ( s_stepAccumulator >= fixedStep && steps < maxSubsteps ) {
		ab_simulate_step( fixedStep );
		s_stepAccumulator -= fixedStep;
		steps++;
	}
}

float ArcBlanc_SampleHeight( float worldX, float worldZ )
{
	float sea;

	if ( !ArcBlanc_Enabled() ) {
		return r_arcBlancSeaLevel ? r_arcBlancSeaLevel->value : 0.0f;
	}
	sea = r_arcBlancSeaLevel ? r_arcBlancSeaLevel->value : 0.0f;
	return AB_Ocean_SampleHeightWorld( &s_ocean, worldX, worldZ ) + sea;
}

void ArcBlanc_SampleVelocity( float worldX, float worldY, float worldZ, vec3_t outVel )
{
	if ( !ArcBlanc_Enabled() ) {
		VectorClear( outVel );
		return;
	}
	AB_Ocean_SampleVelocityWorld( &s_ocean, worldX, worldY, worldZ, outVel );
}

int ArcBlanc_RegisterBoxHull( int physBody, const vec3_t origin, const vec3_t mins, const vec3_t maxs )
{
	int i;
	for ( i = 0; i < AB_MAX_HULLS; i++ ) {
		if ( !s_hulls[i].active ) {
			abHull_t *hull = &s_hulls[i];
			if ( hull->triangles ) {
				Z_Free( hull->triangles );
			}
			Com_Memset( hull, 0, sizeof( *hull ) );
			hull->triangles = (abTriangle_t *)Z_Malloc( 12 * sizeof( abTriangle_t ) );
			hull->cdWater = 0.8f;
			hull->cdAir = 0.05f;
			hull->maskBw = 1.0f;
			ab_build_box_tris( hull, origin, mins, maxs );
			VectorCopy( origin, hull->fdmPrevOrigin );
			VectorCopy( origin, hull->fdmPrev2Origin );
			hull->physBody = physBody;
			hull->active = qtrue;
			return i;
		}
	}
	return -1;
}

void ArcBlanc_UnregisterHull( int hullId )
{
	if ( hullId < 0 || hullId >= AB_MAX_HULLS ) {
		return;
	}
	s_hulls[hullId].active = qfalse;
}

void ArcBlanc_SetHullVelocity( int hullId, const vec3_t velocity )
{
	if ( hullId < 0 || hullId >= AB_MAX_HULLS || !s_hulls[hullId].active ) {
		return;
	}
	VectorCopy( velocity, s_hulls[hullId].velocity );
}

void ArcBlanc_GetHeightExport( arcBlancHeightExport_t *out )
{
	if ( out ) {
		*out = s_heightExport;
	}
}

void ArcBlanc_BuildHeightRGBA( byte *rgba, int maxBytes, int *width, int *height )
{
	int n, i;
	float range, minH, maxH;

	if ( !rgba || !width || !height || !ArcBlanc_Enabled() ) {
		if ( width ) {
			*width = 0;
		}
		if ( height ) {
			*height = 0;
		}
		return;
	}

	n = s_ocean.gridN;
	if ( maxBytes < n * n * 4 ) {
		*width = 0;
		*height = 0;
		return;
	}

	minH = s_heightExport.minHeight;
	maxH = s_heightExport.maxHeight;
	range = maxH - minH;
	if ( range < 1e-4f ) {
		range = 1.0f;
	}

	for ( i = 0; i < n * n; i++ ) {
		byte v = (byte)( 255.0f * ( s_ocean.combinedHeight[i] - minH ) / range );
		rgba[i * 4 + 0] = v;
		rgba[i * 4 + 1] = v;
		rgba[i * 4 + 2] = 255;
		rgba[i * 4 + 3] = 255;
	}
	*width = n;
	*height = n;
}

void ArcBlanc_ResetForTest( void )
{
	AB_Ocean_InitDefaults( &s_ocean, AB_DEFAULT_GRID_N, 256.0f );
	s_stepAccumulator = 0.0f;
	s_ocean.spectrumDirty = qtrue;
	AB_Ocean_UpdateSpectrum( &s_ocean );
	AB_Ocean_UpdateTime( &s_ocean, 0.016f );
}

const abOceanState_t *ArcBlanc_GetOceanForTest( void )
{
	return &s_ocean;
}

void ArcBlanc_Status_f( void )
{
	Com_Printf( "Arc Blanc: %s gpu=%s wind=%.1f fetch=%.0f swell=%.2f dir=%.2f spread=%.2f amp=%.2f height=%.2f chop=%.2f speed=%.2f gust=%.2f hz=%.1f substeps=%d grid=%d tile=%.0f time=%.2f\n",
		ArcBlanc_Enabled() ? "ON" : "OFF",
		( ArcBlanc_GpuWanted() && s_gpuOceanActive ) ? "ON" : "OFF",
		s_ocean.windSpeed, s_ocean.fetch, s_ocean.swell, s_ocean.directional,
		s_ocean.spread, s_ocean.amplitudeScale, s_ocean.heightScale, s_ocean.chopScale,
		s_ocean.waveSpeed, s_ocean.gustStrength,
		r_arcBlancUpdateHz ? r_arcBlancUpdateHz->value : 0.0f,
		r_arcBlancMaxSubsteps ? r_arcBlancMaxSubsteps->integer : 4,
		s_ocean.gridN, s_ocean.tileSize, s_ocean.time );
}

void ArcBlanc_Reseed_f( void )
{
	if ( !ArcBlanc_Enabled() ) {
		Com_Printf( S_COLOR_YELLOW "arc_blanc: enable r_arcBlanc 1 first\n" );
		return;
	}
	s_ocean.spectrumDirty = qtrue;
	AB_Ocean_UpdateSpectrum( &s_ocean );
	Com_Printf( "Arc Blanc spectrum reseeded.\n" );
}

void ArcBlanc_Sample_f( void )
{
	float x, z, h;
	vec3_t vel;

	if ( Cmd_Argc() < 3 ) {
		Com_Printf( "usage: arc_blanc_sample <x> <z>\n" );
		return;
	}
	x = (float)atof( Cmd_Argv( 1 ) );
	z = (float)atof( Cmd_Argv( 2 ) );
	h = ArcBlanc_SampleHeight( x, z );
	ArcBlanc_SampleVelocity( x, h, z, vel );
	Com_Printf( "height=%.3f vel=(%.3f, %.3f, %.3f)\n", h, vel[0], vel[1], vel[2] );
}

void ArcBlanc_Preset_f( void )
{
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "usage: arc_blanc_preset <calm|lake|ocean|storm|cinematic>\n" );
		return;
	}

	if ( !Q_stricmp( Cmd_Argv( 1 ), "calm" ) ) {
		Cvar_Set( "r_arcBlancWind", "8" );
		Cvar_Set( "r_arcBlancFetch", "400" );
		Cvar_Set( "r_arcBlancSwell", "0.15" );
		Cvar_Set( "r_arcBlancDirectional", "0.8" );
		Cvar_Set( "r_arcBlancSpread", "0.15" );
		Cvar_Set( "r_arcBlancAmplitude", "0.55" );
		Cvar_Set( "r_arcBlancHeightScale", "0.75" );
		Cvar_Set( "r_arcBlancChoppiness", "0.55" );
		Cvar_Set( "r_arcBlancWaveSpeed", "0.75" );
		Cvar_Set( "r_arcBlancGustStrength", "0.0" );
		Cvar_Set( "r_arcBlancUpdateHz", "30" );
	} else if ( !Q_stricmp( Cmd_Argv( 1 ), "lake" ) ) {
		Cvar_Set( "r_arcBlancWind", "6" );
		Cvar_Set( "r_arcBlancFetch", "180" );
		Cvar_Set( "r_arcBlancSwell", "0.05" );
		Cvar_Set( "r_arcBlancDirectional", "0.45" );
		Cvar_Set( "r_arcBlancSpread", "0.0" );
		Cvar_Set( "r_arcBlancAmplitude", "0.35" );
		Cvar_Set( "r_arcBlancHeightScale", "0.55" );
		Cvar_Set( "r_arcBlancChoppiness", "0.35" );
		Cvar_Set( "r_arcBlancWaveSpeed", "0.6" );
		Cvar_Set( "r_arcBlancGustStrength", "0.0" );
		Cvar_Set( "r_arcBlancUpdateHz", "24" );
		Cvar_Set( "r_arcBlancLakeMode", "1" );
	} else if ( !Q_stricmp( Cmd_Argv( 1 ), "ocean" ) ) {
		Cvar_Set( "r_arcBlancWind", "20" );
		Cvar_Set( "r_arcBlancFetch", "1000" );
		Cvar_Set( "r_arcBlancSwell", "0.5" );
		Cvar_Set( "r_arcBlancDirectional", "1.0" );
		Cvar_Set( "r_arcBlancSpread", "0.4" );
		Cvar_Set( "r_arcBlancAmplitude", "1.0" );
		Cvar_Set( "r_arcBlancHeightScale", "1.0" );
		Cvar_Set( "r_arcBlancChoppiness", "1.0" );
		Cvar_Set( "r_arcBlancWaveSpeed", "1.0" );
		Cvar_Set( "r_arcBlancGustStrength", "0.15" );
		Cvar_Set( "r_arcBlancUpdateHz", "0" );
		Cvar_Set( "r_arcBlancLakeMode", "0" );
	} else if ( !Q_stricmp( Cmd_Argv( 1 ), "storm" ) ) {
		Cvar_Set( "r_arcBlancWind", "32" );
		Cvar_Set( "r_arcBlancFetch", "2400" );
		Cvar_Set( "r_arcBlancSwell", "0.9" );
		Cvar_Set( "r_arcBlancDirectional", "1.0" );
		Cvar_Set( "r_arcBlancSpread", "0.8" );
		Cvar_Set( "r_arcBlancAmplitude", "1.7" );
		Cvar_Set( "r_arcBlancHeightScale", "1.25" );
		Cvar_Set( "r_arcBlancChoppiness", "1.6" );
		Cvar_Set( "r_arcBlancWaveSpeed", "1.35" );
		Cvar_Set( "r_arcBlancGustStrength", "0.35" );
		Cvar_Set( "r_arcBlancUpdateHz", "60" );
		Cvar_Set( "r_arcBlancLakeMode", "0" );
	} else if ( !Q_stricmp( Cmd_Argv( 1 ), "cinematic" ) ) {
		Cvar_Set( "r_arcBlancWind", "18" );
		Cvar_Set( "r_arcBlancFetch", "1400" );
		Cvar_Set( "r_arcBlancSwell", "0.65" );
		Cvar_Set( "r_arcBlancDirectional", "1.0" );
		Cvar_Set( "r_arcBlancSpread", "0.65" );
		Cvar_Set( "r_arcBlancAmplitude", "1.2" );
		Cvar_Set( "r_arcBlancHeightScale", "1.1" );
		Cvar_Set( "r_arcBlancChoppiness", "1.15" );
		Cvar_Set( "r_arcBlancWaveSpeed", "0.9" );
		Cvar_Set( "r_arcBlancGustStrength", "0.2" );
		Cvar_Set( "r_arcBlancUpdateHz", "120" );
		Cvar_Set( "r_arcBlancMeshDiv", "80" );
		Cvar_Set( "r_arcBlancTileRadius", "2" );
		Cvar_Set( "r_arcBlancLakeMode", "0" );
	} else {
		Com_Printf( S_COLOR_YELLOW "arc_blanc: unknown preset '%s'\n", Cmd_Argv( 1 ) );
		return;
	}

	s_ocean.spectrumDirty = qtrue;
	ab_sync_ocean_params();
	Com_Printf( "Arc Blanc preset applied: %s\n", Cmd_Argv( 1 ) );
}
