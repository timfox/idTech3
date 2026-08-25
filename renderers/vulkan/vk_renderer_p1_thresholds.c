/*
===========================================================================
Phase 1.6 — versioned P1 numerical thresholds.
===========================================================================
*/

#include "tr_local.h"
#include "vk_renderer_p1_thresholds.h"
#include <string.h>


static rendererP1Thresholds_t s_thr;
static qboolean s_inited;

static uint32_t P1_HashMix( uint32_t h, uint32_t v )
{
	h ^= v + 0x9e3779b9u + ( h << 6 ) + ( h >> 2 );
	return h;
}

static uint32_t P1_HashFloat( uint32_t h, float f )
{
	uint32_t u;
	memcpy( &u, &f, sizeof( u ) );
	return P1_HashMix( h, u );
}

static void P1_Thr_Defaults( void )
{
	Com_Memset( &s_thr, 0, sizeof( s_thr ) );
	s_thr.fireflySpikeAttenuationMin = 0.85f;
	s_thr.fireflyCoherentRetentionMin = 0.90f;
	s_thr.fireflyThinLineRetentionMin = 0.70f;
	s_thr.fireflyFalsePositiveMax = 0.25f;
	s_thr.bloomCentroidShiftMaxPx = 1.5f;
	s_thr.bloomRadiusAsymmetryMax = 0.15f;
	s_thr.bloomEnergyGrowthMax = 1.25f;

	s_thr.velocityMeanErrorMax = 1.5f;
	s_thr.velocityMaxErrorMax = 4.0f;
	s_thr.velocityWrongSignFracMax = 0.02f;

	s_thr.ghostTrailLengthMaxPx = 8.0f;
	s_thr.ghostRecoveryFramesMax = 4.0f;
	s_thr.disocclusionContaminationMax = 0.05f;

	s_thr.specularVarianceMax = 0.08f;
	s_thr.specularSpikeCountMax = 64.0f;
	s_thr.specularCloseupSharpnessMin = 0.75f;

	s_thr.gbufferNormalAngularErrorMaxDeg = 5.0f;
	s_thr.gbufferRoughnessAbsErrorMax = 0.08f;
	s_thr.gbufferIdCollisionMax = 0.0f;

	s_thr.lightingMeanRgbErrorMax = 0.02f;
	s_thr.lightingMaxRgbErrorMax = 0.12f;
	s_thr.lightingOwnershipSeamMax = 0.05f;
	s_thr.lightingLightmapErrorMax = 0.04f;

	s_thr.clusterMismatchMax = 0;
	s_thr.clusterOverflowFailMax = 0;

	s_thr.edgeSpreadWidthMaxPx = 4.0f;
	s_thr.edgeContrastRetentionMin = 0.35f;
	s_thr.edgeHaloAmplitudeMax = 0.15f;
	s_thr.smaaMissedEdgeFracMax = 0.10f;

	s_thr.textureTemporalVarianceMax = 0.06f;
	s_thr.textureMoireEnergyMax = 0.20f;
}

static uint32_t P1_Thr_ComputeHash( void )
{
	uint32_t h = 0x116C001u;
	h = P1_HashFloat( h, s_thr.fireflySpikeAttenuationMin );
	h = P1_HashFloat( h, s_thr.fireflyCoherentRetentionMin );
	h = P1_HashFloat( h, s_thr.fireflyThinLineRetentionMin );
	h = P1_HashFloat( h, s_thr.fireflyFalsePositiveMax );
	h = P1_HashFloat( h, s_thr.bloomCentroidShiftMaxPx );
	h = P1_HashFloat( h, s_thr.bloomRadiusAsymmetryMax );
	h = P1_HashFloat( h, s_thr.bloomEnergyGrowthMax );
	h = P1_HashFloat( h, s_thr.velocityMeanErrorMax );
	h = P1_HashFloat( h, s_thr.velocityMaxErrorMax );
	h = P1_HashFloat( h, s_thr.gbufferNormalAngularErrorMaxDeg );
	h = P1_HashFloat( h, s_thr.gbufferRoughnessAbsErrorMax );
	h = P1_HashFloat( h, s_thr.lightingMeanRgbErrorMax );
	h = P1_HashFloat( h, s_thr.lightingMaxRgbErrorMax );
	h = P1_HashFloat( h, s_thr.edgeSpreadWidthMaxPx );
	h = P1_HashFloat( h, s_thr.edgeContrastRetentionMin );
	h = P1_HashMix( h, s_thr.clusterMismatchMax );
	return h;
}

uint32_t vk_renderer_p1_thresholds_hash( void )
{
	if ( !s_inited ) {
		P1_Thr_Defaults();
		s_inited = qtrue;
	}
	return P1_Thr_ComputeHash();
}

const rendererP1Thresholds_t *vk_renderer_p1_thresholds_get( void )
{
	if ( !s_inited ) {
		P1_Thr_Defaults();
		s_inited = qtrue;
	}
	s_thr.contractHash = P1_Thr_ComputeHash();
	return &s_thr;
}

qboolean vk_renderer_p1_thresholds_validate( char *errBuf, int errBufSize )
{
	const rendererP1Thresholds_t *t = vk_renderer_p1_thresholds_get();
	if ( t->fireflySpikeAttenuationMin <= 0.0f || t->fireflySpikeAttenuationMin > 1.0f ) {
		if ( errBuf && errBufSize > 0 ) {
			Com_sprintf( errBuf, errBufSize, "invalid fireflySpikeAttenuationMin" );
		}
		return qfalse;
	}
	if ( t->edgeContrastRetentionMin <= 0.0f ) {
		if ( errBuf && errBufSize > 0 ) {
			Com_sprintf( errBuf, errBufSize, "invalid edgeContrastRetentionMin" );
		}
		return qfalse;
	}
	return qtrue;
}

void vk_renderer_p1_thresholds_export_json( const char *path )
{
	char buf[4096];
	const rendererP1Thresholds_t *t = vk_renderer_p1_thresholds_get();
	int n;
	if ( !path || !path[0] ) {
		path = "render_cert/thresholds.json";
	}
	n = Com_sprintf( buf, sizeof( buf ),
		"{\n"
		"  \"schema\": \"renderer_iq_p1_thresholds\",\n"
		"  \"contractHash\": %u,\n"
		"  \"fireflySpikeAttenuationMin\": %.6g,\n"
		"  \"fireflyCoherentRetentionMin\": %.6g,\n"
		"  \"fireflyFalsePositiveMax\": %.6g,\n"
		"  \"velocityMeanErrorMax\": %.6g,\n"
		"  \"velocityMaxErrorMax\": %.6g,\n"
		"  \"gbufferNormalAngularErrorMaxDeg\": %.6g,\n"
		"  \"gbufferRoughnessAbsErrorMax\": %.6g,\n"
		"  \"lightingMeanRgbErrorMax\": %.6g,\n"
		"  \"lightingMaxRgbErrorMax\": %.6g,\n"
		"  \"edgeSpreadWidthMaxPx\": %.6g,\n"
		"  \"edgeContrastRetentionMin\": %.6g\n"
		"}\n",
		t->contractHash,
		t->fireflySpikeAttenuationMin, t->fireflyCoherentRetentionMin, t->fireflyFalsePositiveMax,
		t->velocityMeanErrorMax, t->velocityMaxErrorMax,
		t->gbufferNormalAngularErrorMaxDeg, t->gbufferRoughnessAbsErrorMax,
		t->lightingMeanRgbErrorMax, t->lightingMaxRgbErrorMax,
		t->edgeSpreadWidthMaxPx, t->edgeContrastRetentionMin );
	ri.FS_WriteFile( path, buf, n );
	ri.Printf( PRINT_ALL, "iq_thresholds_export: wrote %s hash=%u\n", path, t->contractHash );
}

static void P1_Thr_Status_f( void )
{
	const rendererP1Thresholds_t *t = vk_renderer_p1_thresholds_get();
	ri.Printf( PRINT_ALL,
		"=== P1 thresholds (Phase 1.6) hash=%u ===\n"
		"  firefly atten>=%.3f retain>=%.3f fp<=%.3f\n"
		"  velocity mean<=%.3f max<=%.3f\n"
		"  gbuffer ang<=%.3fdeg rough<=%.4f\n"
		"  lighting mean<=%.4f max<=%.4f\n"
		"  edge spread<=%.2fpx contrast>=%.3f\n",
		t->contractHash,
		t->fireflySpikeAttenuationMin, t->fireflyCoherentRetentionMin, t->fireflyFalsePositiveMax,
		t->velocityMeanErrorMax, t->velocityMaxErrorMax,
		t->gbufferNormalAngularErrorMaxDeg, t->gbufferRoughnessAbsErrorMax,
		t->lightingMeanRgbErrorMax, t->lightingMaxRgbErrorMax,
		t->edgeSpreadWidthMaxPx, t->edgeContrastRetentionMin );
}

static void P1_Thr_Export_f( void )
{
	vk_renderer_p1_thresholds_export_json(
		( ri.Cmd_Argc() >= 2 ) ? ri.Cmd_Argv( 1 ) : "render_cert/thresholds.json" );
}

void vk_renderer_p1_thresholds_register( void )
{
	P1_Thr_Defaults();
	s_inited = qtrue;
	if ( ri.Cmd_AddCommand ) {
		ri.Cmd_AddCommand( "iq_thresholds_status", P1_Thr_Status_f );
		ri.Cmd_AddCommand( "iq_thresholds_export", P1_Thr_Export_f );
	}
}

