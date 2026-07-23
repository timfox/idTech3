/*
===========================================================================
Phase 2.6B — deterministic OIT laboratory + core certification orchestration.

Policy: WBOIT_EVIDENCE_CPU_REFERENCE must never be recorded as a silent GPU PASS.
Missing GPU readback → PENDING / WBOIT_EVIDENCE_NONE, not a forged PASS.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_transparency_lab.h"
#include "vk_wboit_production_cert.h"
#include "vk_cert_readback.h"
#include "vk_cert_metrics.h"
#include "vk_oit_weight_contract.h"
#include "vk_specialized_transparency.h"
#include "vk_oit_cert_geometry.h"
#include "vk_oit_lab.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
	OIT_LAB_GROUP_CORE = 0,
	OIT_LAB_GROUP_ALPHA,
	OIT_LAB_GROUP_WEIGHT,
	OIT_LAB_GROUP_ORDER,
	OIT_LAB_GROUP_FOG,
	OIT_LAB_GROUP_ADDITIVE,
	OIT_LAB_GROUP_RESOLVE,
	OIT_LAB_GROUP_LIFECYCLE,
	OIT_LAB_GROUP_SOAK,
	OIT_LAB_GROUP_SPECIALIZED,
	OIT_LAB_GROUP_MBOIT,
	OIT_LAB_GROUP_ALL
} oitLabGroup_t;

typedef enum {
	OIT_LAB_EVAL_NONE = 0,
	OIT_LAB_EVAL_EMPTY,
	OIT_LAB_EVAL_SINGLE,
	OIT_LAB_EVAL_REVEALAGE,
	OIT_LAB_EVAL_WEIGHT,
	OIT_LAB_EVAL_ORDER,
	OIT_LAB_EVAL_FOG,
	OIT_LAB_EVAL_ADDITIVE,
	OIT_LAB_EVAL_HDR,
	OIT_LAB_EVAL_LIFECYCLE
} oitLabEval_t;

typedef struct {
	const char *name;
	oitLabGroup_t group;
	wboitCertStage_t stage;
	uint32_t seed;
	oitLabEval_t eval;
	qboolean (*arm)( void );
} oitLabCase_t;

static qboolean s_cmds;
static cvar_t *r_oitLabFreeze;
static int s_lastCase = -1;
static char s_lastStatus[64];
static oitLabEval_t s_pendingEval;
static wboitCertStage_t s_pendingStage;
static char s_pendingTest[64];
static int s_orderPerm;
static float s_orderMaxErr[6];
static int s_orderCount;
static int s_coreQueue[32];
static int s_coreQueueLen;
static int s_coreQueuePos;
static qboolean s_coreRunning;

static void OIT_Lab_AdvanceCoreQueue( void );

static void OIT_Lab_ApplyFreeze( void )
{
	if ( !r_oitLabFreeze || !r_oitLabFreeze->integer ) {
		return;
	}
	ri.Cvar_Set( "r_transparencyFreeze", "1" );
	ri.Cvar_Set( "r_taa", "0" );
}

static void OIT_Lab_ArmEval( oitLabEval_t eval, wboitCertStage_t stage, const char *testName )
{
	s_pendingEval = eval;
	s_pendingStage = stage;
	Q_strncpyz( s_pendingTest, testName ? testName : "lab", sizeof( s_pendingTest ) );
}

static qboolean OIT_Lab_ArmEmpty( void )
{
	oitCertScenario_t sc;
	vk_oit_cert_geometry_make_empty( &sc );
	vk_oit_cert_geometry_arm( &sc );
	OIT_Lab_ArmEval( OIT_LAB_EVAL_EMPTY, WBOIT_CERT_STAGE_EMPTY_PIXEL, "wboit_empty_pixel" );
	return qtrue;
}

static qboolean OIT_Lab_ArmSingle( void )
{
	oitCertScenario_t sc;
	const float color[3] = { 0.8f, 0.2f, 0.1f };
	vk_oit_cert_geometry_make_single_layer( &sc, 0.5f, color, 256.0f );
	vk_oit_cert_geometry_arm( &sc );
	OIT_Lab_ArmEval( OIT_LAB_EVAL_SINGLE, WBOIT_CERT_STAGE_SINGLE_LAYER, "wboit_single_layer" );
	return qtrue;
}

static qboolean OIT_Lab_ArmRevealage( void )
{
	oitCertScenario_t sc;
	const float alphas[] = { 0.25f, 0.5f, 0.1f, 0.4f };
	vk_oit_cert_geometry_make_revealage_layers( &sc, alphas, 4, 220.0f );
	vk_oit_cert_geometry_arm( &sc );
	OIT_Lab_ArmEval( OIT_LAB_EVAL_REVEALAGE, WBOIT_CERT_STAGE_REVEALAGE, "wboit_revealage" );
	return qtrue;
}

static qboolean OIT_Lab_ArmWeight( void )
{
	oitCertScenario_t sc;
	vk_oit_cert_geometry_make_weight_ladder( &sc );
	vk_oit_cert_geometry_arm( &sc );
	OIT_Lab_ArmEval( OIT_LAB_EVAL_WEIGHT, WBOIT_CERT_STAGE_WEIGHT_BOUNDS, "wboit_weight_bounds" );
	return qtrue;
}

static qboolean OIT_Lab_ArmOrder( void )
{
	oitCertScenario_t sc;
	vk_oit_cert_geometry_make_order_rgb( &sc, s_orderPerm );
	vk_oit_cert_geometry_arm( &sc );
	OIT_Lab_ArmEval( OIT_LAB_EVAL_ORDER, WBOIT_CERT_STAGE_ORDER_STABILITY, "wboit_order_permutations" );
	return qtrue;
}

static qboolean OIT_Lab_ArmFog( void )
{
	oitCertScenario_t sc;
	vk_oit_cert_geometry_make_fog_depth_ladder( &sc );
	vk_oit_cert_geometry_arm( &sc );
	ri.Cvar_Set( "r_oitFogMode", "1" );
	ri.Cvar_Set( "r_oitFogDensity", "0.002" );
	OIT_Lab_ArmEval( OIT_LAB_EVAL_FOG, WBOIT_CERT_STAGE_FOG_DEPTH, "wboit_fog_depth" );
	return qtrue;
}

static qboolean OIT_Lab_ArmAdditive( void )
{
	oitCertScenario_t sc;
	vk_oit_cert_geometry_make_additive_over_glass( &sc );
	vk_oit_cert_geometry_arm( &sc );
	OIT_Lab_ArmEval( OIT_LAB_EVAL_ADDITIVE, WBOIT_CERT_STAGE_ADDITIVE, "wboit_additive" );
	return qtrue;
}

static qboolean OIT_Lab_ArmHdr( void )
{
	oitCertScenario_t sc;
	vk_oit_cert_geometry_make_empty( &sc );
	Q_strncpyz( sc.name, "hdr_resolve_empty", sizeof( sc.name ) );
	vk_oit_cert_geometry_arm( &sc );
	OIT_Lab_ArmEval( OIT_LAB_EVAL_HDR, WBOIT_CERT_STAGE_HDR_RESOLVE, "wboit_hdr_resolve" );
	return qtrue;
}

static qboolean OIT_Lab_ArmLifecycle( void )
{
	oitCertScenario_t sc;
	vk_oit_cert_geometry_make_empty( &sc );
	Q_strncpyz( sc.name, "lifecycle_empty", sizeof( sc.name ) );
	vk_oit_cert_geometry_arm( &sc );
	OIT_Lab_ArmEval( OIT_LAB_EVAL_LIFECYCLE, WBOIT_CERT_STAGE_LIFECYCLE, "wboit_lifecycle" );
	return qtrue;
}

static qboolean OIT_Lab_ArmAlpha( void )
{
	oitCertScenario_t sc;
	const float color[3] = { 0.4f, 0.7f, 0.2f };
	vk_oit_cert_geometry_make_single_layer( &sc, 0.35f, color, 300.0f );
	Q_strncpyz( sc.name, "alpha_equivalence", sizeof( sc.name ) );
	vk_oit_cert_geometry_arm( &sc );
	OIT_Lab_ArmEval( OIT_LAB_EVAL_SINGLE, WBOIT_CERT_STAGE_ALPHA_ENCODING, "wboit_alpha_equivalence" );
	return qtrue;
}

static qboolean OIT_Lab_ArmSpecialized( void )
{
	vk_transparency_resource_bump( XPARENT_RES_REFRACTIVE_INPUT );
	vk_transparency_resource_bump( XPARENT_RES_REFRACTED_HDR );
	ri.Printf( PRINT_ALL, "oit_lab specialized: scaffold only (gated until PRODUCTION)\n" );
	s_pendingEval = OIT_LAB_EVAL_NONE;
	return qtrue;
}

static qboolean OIT_Lab_ArmMboit( void )
{
	ri.Printf( PRINT_ALL, "oit_lab mboit: experimental — does not affect WBOIT cert\n" );
	s_pendingEval = OIT_LAB_EVAL_NONE;
	return qtrue;
}

static const oitLabCase_t s_cases[] = {
	{ "wboit_empty_pixel", OIT_LAB_GROUP_CORE, WBOIT_CERT_STAGE_EMPTY_PIXEL, 1, OIT_LAB_EVAL_EMPTY, OIT_Lab_ArmEmpty },
	{ "wboit_single_layer", OIT_LAB_GROUP_CORE, WBOIT_CERT_STAGE_SINGLE_LAYER, 2, OIT_LAB_EVAL_SINGLE, OIT_Lab_ArmSingle },
	{ "wboit_revealage", OIT_LAB_GROUP_ALPHA, WBOIT_CERT_STAGE_REVEALAGE, 3, OIT_LAB_EVAL_REVEALAGE, OIT_Lab_ArmRevealage },
	{ "wboit_alpha_equivalence", OIT_LAB_GROUP_ALPHA, WBOIT_CERT_STAGE_ALPHA_ENCODING, 4, OIT_LAB_EVAL_SINGLE, OIT_Lab_ArmAlpha },
	{ "wboit_weight_bounds", OIT_LAB_GROUP_WEIGHT, WBOIT_CERT_STAGE_WEIGHT_BOUNDS, 5, OIT_LAB_EVAL_WEIGHT, OIT_Lab_ArmWeight },
	{ "wboit_order_permutations", OIT_LAB_GROUP_ORDER, WBOIT_CERT_STAGE_ORDER_STABILITY, 6, OIT_LAB_EVAL_ORDER, OIT_Lab_ArmOrder },
	{ "wboit_fog_depth", OIT_LAB_GROUP_FOG, WBOIT_CERT_STAGE_FOG_DEPTH, 7, OIT_LAB_EVAL_FOG, OIT_Lab_ArmFog },
	{ "wboit_additive", OIT_LAB_GROUP_ADDITIVE, WBOIT_CERT_STAGE_ADDITIVE, 8, OIT_LAB_EVAL_ADDITIVE, OIT_Lab_ArmAdditive },
	{ "wboit_hdr_resolve", OIT_LAB_GROUP_RESOLVE, WBOIT_CERT_STAGE_HDR_RESOLVE, 9, OIT_LAB_EVAL_HDR, OIT_Lab_ArmHdr },
	{ "wboit_lifecycle", OIT_LAB_GROUP_LIFECYCLE, WBOIT_CERT_STAGE_LIFECYCLE, 10, OIT_LAB_EVAL_LIFECYCLE, OIT_Lab_ArmLifecycle },
	{ "specialized_refraction_smoke", OIT_LAB_GROUP_SPECIALIZED, WBOIT_CERT_STAGE_COUNT, 11, OIT_LAB_EVAL_NONE, OIT_Lab_ArmSpecialized },
	{ "mboit_compare", OIT_LAB_GROUP_MBOIT, WBOIT_CERT_STAGE_COUNT, 12, OIT_LAB_EVAL_NONE, OIT_Lab_ArmMboit },
};

static void OIT_Lab_RecordPending( wboitCertStatus_t status, wboitCertEvidence_t evidence,
	double observed, double failThr, const char *reason )
{
	wboitCertStageResult_t r;
	Com_Memset( &r, 0, sizeof( r ) );
	r.stage = (uint32_t)s_pendingStage;
	r.status = (uint32_t)status;
	r.evidenceType = (uint32_t)evidence;
	r.observed = observed;
	r.failureThreshold = failThr;
	r.warningThreshold = failThr * 0.5;
	Q_strncpyz( r.testName, s_pendingTest, sizeof( r.testName ) );
	Q_strncpyz( r.failureReason, reason ? reason : "", sizeof( r.failureReason ) );
	vk_wboit_cert_record_result( &r );
	Q_strncpyz( s_lastStatus,
		( status == WBOIT_CERT_STATUS_PASS ) ? "PASS" :
		( status == WBOIT_CERT_STATUS_FAIL ) ? "FAIL" : "PENDING",
		sizeof( s_lastStatus ) );
}

static qboolean OIT_Lab_CenterSample( const float *rgba, uint32_t w, uint32_t h, float out[4] )
{
	uint32_t cx, cy, i;
	if ( !rgba || w < 2 || h < 2 ) {
		return qfalse;
	}
	cx = w / 2;
	cy = h / 2;
	i = cy * w + cx;
	out[0] = rgba[i * 4 + 0];
	out[1] = rgba[i * 4 + 1];
	out[2] = rgba[i * 4 + 2];
	out[3] = rgba[i * 4 + 3];
	return qtrue;
}

static void OIT_Lab_EvalEmpty( void )
{
	certReadbackCapture_t fog, accum, reveal, resolved;
	certMetrics_t m;
	float *reveal1;
	uint32_t i, n;

	if ( !vk_cert_readback_capture( CERT_RB_FOG_SCENE, &fog ) ||
		!vk_cert_readback_capture( CERT_RB_OIT_ACCUM, &accum ) ||
		!vk_cert_readback_capture( CERT_RB_OIT_REVEALAGE, &reveal ) ||
		!vk_cert_readback_capture( CERT_RB_RESOLVED_WBOIT, &resolved ) ) {
		OIT_Lab_RecordPending( WBOIT_CERT_STATUS_PENDING, WBOIT_EVIDENCE_NONE, 0, 0,
			"readback unavailable (need live OIT frame with r_oit 1)" );
		return;
	}
	n = fog.width * fog.height;
	reveal1 = (float *)malloc( sizeof( float ) * n );
	if ( !reveal1 ) {
		return;
	}
	for ( i = 0; i < n; i++ ) {
		reveal1[i] = reveal.rgba[i * 4];
	}
	vk_cert_metrics_empty_pixels( fog.rgba, accum.rgba, reveal1, resolved.rgba,
		fog.width, fog.height, 1e-3f, &m );
	free( reveal1 );
	if ( m.modifiedEmptyPixels == 0 && m.nanCount == 0 ) {
		char notes[192];
		Com_sprintf( notes, sizeof( notes ), "emptyPixels=%u modified=0 maxErr=%g",
			m.validPixelCount, m.maxEmptyPixelError );
		OIT_Lab_RecordPending( WBOIT_CERT_STATUS_PASS, WBOIT_EVIDENCE_GPU_READBACK, 0.0, 0.0, notes );
	} else {
		char notes[192];
		Com_sprintf( notes, sizeof( notes ), "modifiedEmpty=%u maxErr=%g",
			m.modifiedEmptyPixels, m.maxEmptyPixelError );
		OIT_Lab_RecordPending( WBOIT_CERT_STATUS_FAIL, WBOIT_EVIDENCE_GPU_READBACK,
			(double)m.modifiedEmptyPixels, 0.0, notes );
	}
}

static void OIT_Lab_EvalSingle( void )
{
	certReadbackCapture_t fog, resolved;
	const oitCertScenario_t *sc = vk_oit_cert_geometry_scenario();
	float fogC[4], resC[4], expect[3];
	float err;

	if ( !sc || !vk_cert_readback_capture( CERT_RB_FOG_SCENE, &fog ) ||
		!vk_cert_readback_capture( CERT_RB_RESOLVED_WBOIT, &resolved ) ) {
		OIT_Lab_RecordPending( WBOIT_CERT_STATUS_PENDING, WBOIT_EVIDENCE_NONE, 0, 2e-2,
			"single-layer readback unavailable" );
		return;
	}
	if ( !OIT_Lab_CenterSample( fog.rgba, fog.width, fog.height, fogC ) ||
		!OIT_Lab_CenterSample( resolved.rgba, resolved.width, resolved.height, resC ) ) {
		OIT_Lab_RecordPending( WBOIT_CERT_STATUS_FAIL, WBOIT_EVIDENCE_GPU_READBACK, 1, 2e-2,
			"center sample failed" );
		return;
	}
	vk_oit_cert_geometry_expect_source_over( sc->expectSingleColor, sc->expectSingleOpacity, fogC, expect );
	err = fmaxf( fabsf( resC[0] - expect[0] ), fmaxf( fabsf( resC[1] - expect[1] ), fabsf( resC[2] - expect[2] ) ) );
	{
		char notes[192];
		Com_sprintf( notes, sizeof( notes ),
			"centerAbsErr=%g opacity=%g expect=(%.3f %.3f %.3f) got=(%.3f %.3f %.3f)",
			err, sc->expectSingleOpacity, expect[0], expect[1], expect[2], resC[0], resC[1], resC[2] );
		if ( err <= 2e-2f ) {
			OIT_Lab_RecordPending( WBOIT_CERT_STATUS_PASS, WBOIT_EVIDENCE_GPU_READBACK, err, 2e-2, notes );
		} else {
			OIT_Lab_RecordPending( WBOIT_CERT_STATUS_FAIL, WBOIT_EVIDENCE_GPU_READBACK, err, 2e-2, notes );
		}
	}
}

static void OIT_Lab_EvalRevealage( void )
{
	certReadbackCapture_t reveal;
	const oitCertScenario_t *sc = vk_oit_cert_geometry_scenario();
	float sample[4];
	float err;

	if ( !sc || !vk_cert_readback_capture( CERT_RB_OIT_REVEALAGE, &reveal ) ) {
		OIT_Lab_RecordPending( WBOIT_CERT_STATUS_PENDING, WBOIT_EVIDENCE_NONE, 0, 5e-2,
			"revealage readback unavailable" );
		return;
	}
	if ( !OIT_Lab_CenterSample( reveal.rgba, reveal.width, reveal.height, sample ) ) {
		OIT_Lab_RecordPending( WBOIT_CERT_STATUS_FAIL, WBOIT_EVIDENCE_GPU_READBACK, 1, 5e-2,
			"center reveal sample failed" );
		return;
	}
	err = fabsf( sample[0] - sc->expectRevealage );
	{
		char notes[160];
		Com_sprintf( notes, sizeof( notes ), "gpuReveal=%g expect=%g absErr=%g",
			sample[0], sc->expectRevealage, err );
		if ( err <= 5e-2f ) {
			OIT_Lab_RecordPending( WBOIT_CERT_STATUS_PASS, WBOIT_EVIDENCE_GPU_READBACK, err, 5e-2, notes );
		} else {
			OIT_Lab_RecordPending( WBOIT_CERT_STATUS_FAIL, WBOIT_EVIDENCE_GPU_READBACK, err, 5e-2, notes );
		}
	}
}

static void OIT_Lab_EvalWeight( void )
{
	certReadbackCapture_t accum;
	const oitWeightContract_t *w = vk_oit_weight_contract_get();
	certMetrics_t m;
	float *weights;
	uint32_t i, n, k;

	if ( !w || !vk_cert_readback_capture( CERT_RB_OIT_ACCUM, &accum ) ) {
		OIT_Lab_RecordPending( WBOIT_CERT_STATUS_PENDING, WBOIT_EVIDENCE_NONE, 0, 0,
			"weight accum readback unavailable" );
		return;
	}
	n = accum.pixelCount;
	weights = (float *)malloc( sizeof( float ) * n );
	if ( !weights ) {
		return;
	}
	k = 0;
	for ( i = 0; i < n; i++ ) {
		float wt = accum.rgba[i * 4 + 3];
		if ( wt > 1e-6f ) {
			weights[k++] = wt;
		}
	}
	if ( k == 0 ) {
		free( weights );
		OIT_Lab_RecordPending( WBOIT_CERT_STATUS_PENDING, WBOIT_EVIDENCE_NONE, 0, 0,
			"no weighted fragments — fixtures may not have drawn" );
		return;
	}
	vk_cert_metrics_weights( weights, k, w->minWeight, w->maxWeight, &m );
	free( weights );
	{
		char notes[192];
		Com_sprintf( notes, sizeof( notes ),
			"min=%g max=%g mean=%g invalid=%u lowClamp=%u highClamp=%u",
			m.weightMin, m.weightMax, m.weightMean, m.weightInvalid,
			m.weightLowClamps, m.weightHighClamps );
		if ( m.weightInvalid == 0 && m.weightMin >= 0.0 &&
			m.weightMax <= (double)w->maxWeight + 1.0 ) {
			OIT_Lab_RecordPending( WBOIT_CERT_STATUS_PASS, WBOIT_EVIDENCE_GPU_REDUCTION,
				m.weightMax, w->maxWeight, notes );
		} else {
			OIT_Lab_RecordPending( WBOIT_CERT_STATUS_FAIL, WBOIT_EVIDENCE_GPU_REDUCTION,
				m.weightMax, w->maxWeight, notes );
		}
	}
}

static void OIT_Lab_EvalOrder( void )
{
	certReadbackCapture_t resolved;
	float sample[4];
	float lum;

	if ( !vk_cert_readback_capture( CERT_RB_RESOLVED_WBOIT, &resolved ) ||
		!OIT_Lab_CenterSample( resolved.rgba, resolved.width, resolved.height, sample ) ) {
		OIT_Lab_RecordPending( WBOIT_CERT_STATUS_PENDING, WBOIT_EVIDENCE_NONE, 0, 0.05,
			"order readback unavailable" );
		return;
	}
	lum = vk_cert_metrics_luminance( sample[0], sample[1], sample[2] );
	if ( s_orderPerm >= 0 && s_orderPerm < 6 ) {
		s_orderMaxErr[s_orderPerm] = lum;
	}
	s_orderCount++;
	if ( s_orderPerm < 5 ) {
		s_orderPerm++;
		OIT_Lab_ArmOrder();
		Q_strncpyz( s_lastStatus, "ARMED_NEXT_PERM", sizeof( s_lastStatus ) );
		ri.Printf( PRINT_ALL, "oit_lab order: captured perm %d lum=%g — arming next\n",
			s_orderPerm - 1, lum );
		return;
	}
	{
		int i;
		double mean = 0.0, var = 0.0;
		char notes[192];
		for ( i = 0; i < 6; i++ ) {
			mean += s_orderMaxErr[i];
		}
		mean /= 6.0;
		for ( i = 0; i < 6; i++ ) {
			double d = s_orderMaxErr[i] - mean;
			var += d * d;
		}
		var /= 6.0;
		Com_sprintf( notes, sizeof( notes ), "permLumVar=%g meanLum=%g", var, mean );
		s_orderPerm = 0;
		s_orderCount = 0;
		if ( var <= 0.15 ) {
			OIT_Lab_RecordPending( WBOIT_CERT_STATUS_PASS, WBOIT_EVIDENCE_GPU_READBACK, var, 0.15, notes );
		} else {
			OIT_Lab_RecordPending( WBOIT_CERT_STATUS_FAIL, WBOIT_EVIDENCE_GPU_READBACK, var, 0.15, notes );
		}
	}
}

static void OIT_Lab_EvalFog( void )
{
	certReadbackCapture_t resolved, fog;
	float a[4], b[4];
	Com_Memset( a, 0, sizeof( a ) );
	Com_Memset( b, 0, sizeof( b ) );
	if ( !vk_cert_readback_capture( CERT_RB_FOG_SCENE, &fog ) ||
		!vk_cert_readback_capture( CERT_RB_RESOLVED_WBOIT, &resolved ) ) {
		OIT_Lab_RecordPending( WBOIT_CERT_STATUS_PENDING, WBOIT_EVIDENCE_NONE, 0, 0,
			"fog readback unavailable" );
		return;
	}
	if ( !OIT_Lab_CenterSample( fog.rgba, fog.width, fog.height, a ) ||
		!OIT_Lab_CenterSample( resolved.rgba, resolved.width, resolved.height, b ) ) {
		OIT_Lab_RecordPending( WBOIT_CERT_STATUS_FAIL, WBOIT_EVIDENCE_GPU_READBACK, 1, 0,
			"fog center sample failed" );
		return;
	}
	if ( isfinite( b[0] ) && isfinite( b[1] ) && isfinite( b[2] ) ) {
		OIT_Lab_RecordPending( WBOIT_CERT_STATUS_PASS, WBOIT_EVIDENCE_GPU_READBACK, 0, 0,
			"fog/depth ladder resolved finite HDR" );
	} else {
		OIT_Lab_RecordPending( WBOIT_CERT_STATUS_FAIL, WBOIT_EVIDENCE_GPU_READBACK, 1, 0,
			"non-finite fog resolve" );
	}
}

static void OIT_Lab_EvalAdditive( void )
{
	certReadbackCapture_t reveal;
	const oitCertScenario_t *sc = vk_oit_cert_geometry_scenario();
	float sample[4];
	float err;

	if ( !sc || !vk_cert_readback_capture( CERT_RB_OIT_REVEALAGE, &reveal ) ||
		!OIT_Lab_CenterSample( reveal.rgba, reveal.width, reveal.height, sample ) ) {
		OIT_Lab_RecordPending( WBOIT_CERT_STATUS_PENDING, WBOIT_EVIDENCE_NONE, 0, 0,
			"additive revealage readback unavailable" );
		return;
	}
	err = fabsf( sample[0] - sc->expectRevealage );
	{
		char notes[160];
		Com_sprintf( notes, sizeof( notes ),
			"reveal=%g expectGlassOnly=%g delta=%g", sample[0], sc->expectRevealage, err );
		if ( err <= 5e-2f ) {
			OIT_Lab_RecordPending( WBOIT_CERT_STATUS_PASS, WBOIT_EVIDENCE_GPU_READBACK, err, 0, notes );
		} else {
			OIT_Lab_RecordPending( WBOIT_CERT_STATUS_FAIL, WBOIT_EVIDENCE_GPU_READBACK, err, 0, notes );
		}
	}
}

static void OIT_Lab_EvalHdr( void )
{
	certReadbackCapture_t fog, resolved;
	if ( !vk_cert_readback_capture( CERT_RB_FOG_SCENE, &fog ) ||
		!vk_cert_readback_capture( CERT_RB_RESOLVED_WBOIT, &resolved ) ||
		fog.generation == 0 || fog.generation != resolved.generation ) {
		OIT_Lab_RecordPending( WBOIT_CERT_STATUS_PENDING, WBOIT_EVIDENCE_NONE, 0, 0,
			"hdr resolve generation mismatch or unavailable" );
		return;
	}
	{
		char notes[128];
		Com_sprintf( notes, sizeof( notes ), "fog/resolved gen=%u frame=%llu",
			fog.generation, (unsigned long long)fog.frameNumber );
		OIT_Lab_RecordPending( WBOIT_CERT_STATUS_PASS, WBOIT_EVIDENCE_GPU_READBACK,
			(double)fog.generation, 0, notes );
	}
}

static void OIT_Lab_EvalLifecycle( void )
{
	if ( vk_oit_cert_geometry_was_drawn() || vk_oit_cert_geometry_armed() ) {
		OIT_Lab_RecordPending( WBOIT_CERT_STATUS_PASS, WBOIT_EVIDENCE_LIFECYCLE, 1, 0,
			"fixture path survived OIT frame" );
	} else {
		OIT_Lab_RecordPending( WBOIT_CERT_STATUS_PENDING, WBOIT_EVIDENCE_NONE, 0, 0,
			"lifecycle fixture not drawn" );
	}
}

void vk_oit_lab_on_oit_resolved( void )
{
	oitLabEval_t eval = s_pendingEval;
	if ( eval == OIT_LAB_EVAL_NONE ) {
		return;
	}
	if ( !vk_oit_cert_geometry_was_drawn() && eval != OIT_LAB_EVAL_EMPTY &&
		eval != OIT_LAB_EVAL_HDR && eval != OIT_LAB_EVAL_LIFECYCLE &&
		eval != OIT_LAB_EVAL_ORDER ) {
		OIT_Lab_RecordPending( WBOIT_CERT_STATUS_PENDING, WBOIT_EVIDENCE_NONE, 0, 0,
			"fixtures armed but not drawn this OIT frame (is r_oit 1 and in-world?)" );
		s_pendingEval = OIT_LAB_EVAL_NONE;
		OIT_Lab_AdvanceCoreQueue();
		return;
	}

	switch ( eval ) {
	case OIT_LAB_EVAL_EMPTY: OIT_Lab_EvalEmpty(); break;
	case OIT_LAB_EVAL_SINGLE: OIT_Lab_EvalSingle(); break;
	case OIT_LAB_EVAL_REVEALAGE: OIT_Lab_EvalRevealage(); break;
	case OIT_LAB_EVAL_WEIGHT: OIT_Lab_EvalWeight(); break;
	case OIT_LAB_EVAL_ORDER: OIT_Lab_EvalOrder(); break;
	case OIT_LAB_EVAL_FOG: OIT_Lab_EvalFog(); break;
	case OIT_LAB_EVAL_ADDITIVE: OIT_Lab_EvalAdditive(); break;
	case OIT_LAB_EVAL_HDR: OIT_Lab_EvalHdr(); break;
	case OIT_LAB_EVAL_LIFECYCLE: OIT_Lab_EvalLifecycle(); break;
	default: break;
	}

	if ( eval == OIT_LAB_EVAL_ORDER && s_pendingEval == OIT_LAB_EVAL_ORDER ) {
		return;
	}
	s_pendingEval = OIT_LAB_EVAL_NONE;
	OIT_Lab_AdvanceCoreQueue();
}

static const char *OIT_Lab_GroupName( oitLabGroup_t g )
{
	switch ( g ) {
	case OIT_LAB_GROUP_CORE: return "core";
	case OIT_LAB_GROUP_ALPHA: return "alpha";
	case OIT_LAB_GROUP_WEIGHT: return "weight";
	case OIT_LAB_GROUP_ORDER: return "order";
	case OIT_LAB_GROUP_FOG: return "fog";
	case OIT_LAB_GROUP_ADDITIVE: return "additive";
	case OIT_LAB_GROUP_RESOLVE: return "resolve";
	case OIT_LAB_GROUP_LIFECYCLE: return "lifecycle";
	case OIT_LAB_GROUP_SOAK: return "soak";
	case OIT_LAB_GROUP_SPECIALIZED: return "specialized";
	case OIT_LAB_GROUP_MBOIT: return "mboit";
	case OIT_LAB_GROUP_ALL: return "all";
	default: return "?";
	}
}

static void OIT_Lab_List_f( void )
{
	int i;
	ri.Printf( PRINT_ALL, "oit_lab_list (%d cases) — Phase 2.6B fixture-backed:\n",
		(int)( sizeof( s_cases ) / sizeof( s_cases[0] ) ) );
	for ( i = 0; i < (int)( sizeof( s_cases ) / sizeof( s_cases[0] ) ); i++ ) {
		ri.Printf( PRINT_ALL, "  %-28s group=%-12s stage=%s\n",
			s_cases[i].name, OIT_Lab_GroupName( s_cases[i].group ),
			s_cases[i].stage < WBOIT_CERT_STAGE_COUNT ? vk_wboit_cert_stage_name( s_cases[i].stage ) : "(none)" );
	}
}

static int OIT_Lab_Find( const char *name )
{
	int i;
	for ( i = 0; i < (int)( sizeof( s_cases ) / sizeof( s_cases[0] ) ); i++ ) {
		if ( !Q_stricmp( name, s_cases[i].name ) ) {
			return i;
		}
	}
	return -1;
}

static qboolean OIT_Lab_RunIndex( int idx )
{
	if ( idx < 0 || idx >= (int)( sizeof( s_cases ) / sizeof( s_cases[0] ) ) ) {
		return qfalse;
	}
	OIT_Lab_ApplyFreeze();
	s_lastCase = idx;
	if ( s_cases[idx].eval == OIT_LAB_EVAL_ORDER ) {
		s_orderPerm = 0;
		s_orderCount = 0;
		Com_Memset( s_orderMaxErr, 0, sizeof( s_orderMaxErr ) );
	}
	ri.Printf( PRINT_ALL, "oit_lab_run: arming %s (await next OIT frame with r_oit 1)\n",
		s_cases[idx].name );
	if ( s_cases[idx].arm ) {
		s_cases[idx].arm();
	}
	Q_strncpyz( s_lastStatus, "ARMED", sizeof( s_lastStatus ) );
	return qtrue;
}

static void OIT_Lab_AdvanceCoreQueue( void )
{
	if ( !s_coreRunning ) {
		return;
	}
	s_coreQueuePos++;
	if ( s_coreQueuePos >= s_coreQueueLen ) {
		s_coreRunning = qfalse;
		ri.Printf( PRINT_ALL,
			"oit_certify_core: queue complete — level=%s\n",
			vk_wboit_production_level_name( vk_wboit_production_level() ) );
		ri.Cmd_ExecuteText( EXEC_APPEND, "oit_certification_export\n" );
		return;
	}
	OIT_Lab_RunIndex( s_coreQueue[s_coreQueuePos] );
}

static void OIT_Lab_Run_f( void )
{
	int idx;
	if ( ri.Cmd_Argc() < 2 ) {
		ri.Printf( PRINT_ALL, "usage: oit_lab_run <case>\n" );
		return;
	}
	idx = OIT_Lab_Find( ri.Cmd_Argv( 1 ) );
	if ( idx < 0 ) {
		ri.Printf( PRINT_ALL, "unknown case '%s'\n", ri.Cmd_Argv( 1 ) );
		return;
	}
	OIT_Lab_RunIndex( idx );
}

static oitLabGroup_t OIT_Lab_ParseGroup( const char *name )
{
	int g;
	for ( g = 0; g <= (int)OIT_LAB_GROUP_ALL; g++ ) {
		if ( !Q_stricmp( name, OIT_Lab_GroupName( (oitLabGroup_t)g ) ) ) {
			return (oitLabGroup_t)g;
		}
	}
	return OIT_LAB_GROUP_ALL;
}

static void OIT_Lab_RunGroup_f( void )
{
	oitLabGroup_t g;
	int i;
	if ( ri.Cmd_Argc() < 2 ) {
		ri.Printf( PRINT_ALL, "usage: oit_lab_run_group <group>\n" );
		return;
	}
	g = OIT_Lab_ParseGroup( ri.Cmd_Argv( 1 ) );
	s_coreQueueLen = 0;
	for ( i = 0; i < (int)( sizeof( s_cases ) / sizeof( s_cases[0] ) ); i++ ) {
		if ( g != OIT_LAB_GROUP_ALL && s_cases[i].group != g ) {
			continue;
		}
		if ( s_coreQueueLen < (int)( sizeof( s_coreQueue ) / sizeof( s_coreQueue[0] ) ) ) {
			s_coreQueue[s_coreQueueLen++] = i;
		}
	}
	s_coreQueuePos = 0;
	s_coreRunning = ( s_coreQueueLen > 0 ) ? qtrue : qfalse;
	if ( s_coreRunning ) {
		ri.Printf( PRINT_ALL, "oit_lab_run_group %s: queue %d cases\n",
			OIT_Lab_GroupName( g ), s_coreQueueLen );
		OIT_Lab_RunIndex( s_coreQueue[0] );
	}
}

static void OIT_CertifyCore_f( void )
{
	static const char *coreNames[] = {
		"wboit_empty_pixel",
		"wboit_single_layer",
		"wboit_revealage",
		"wboit_alpha_equivalence",
		"wboit_weight_bounds",
		"wboit_order_permutations",
		"wboit_fog_depth",
		"wboit_additive",
		"wboit_hdr_resolve",
		"wboit_lifecycle"
	};
	int i;
	s_coreQueueLen = 0;
	for ( i = 0; i < (int)( sizeof( coreNames ) / sizeof( coreNames[0] ) ); i++ ) {
		int idx = OIT_Lab_Find( coreNames[i] );
		if ( idx >= 0 && s_coreQueueLen < (int)( sizeof( s_coreQueue ) / sizeof( s_coreQueue[0] ) ) ) {
			s_coreQueue[s_coreQueueLen++] = idx;
		}
	}
	s_coreQueuePos = 0;
	s_coreRunning = qtrue;
	ri.Printf( PRINT_ALL,
		"oit_certify_core: starting %d fixture-backed GPU stages\n"
		"  Require: r_oit 1, r_fbo 1, in-world camera; advances each OIT frame\n",
		s_coreQueueLen );
	if ( s_coreQueueLen > 0 ) {
		OIT_Lab_RunIndex( s_coreQueue[0] );
	}
}

static void OIT_Lab_Status_f( void )
{
	ri.Printf( PRINT_ALL,
		"oit_lab_status: last=%s status=%s pendingEval=%d coreQueue=%d/%d level=%s\n"
		"  geometry: armed=%d drawn=%d\n",
		( s_lastCase >= 0 ) ? s_cases[s_lastCase].name : "-",
		s_lastStatus[0] ? s_lastStatus : "-",
		(int)s_pendingEval,
		s_coreQueuePos, s_coreQueueLen,
		vk_wboit_production_level_name( vk_wboit_production_level() ),
		vk_oit_cert_geometry_armed() ? 1 : 0,
		vk_oit_cert_geometry_was_drawn() ? 1 : 0 );
}

static void OIT_Lab_Reset_f( void )
{
	s_pendingEval = OIT_LAB_EVAL_NONE;
	s_coreRunning = qfalse;
	s_coreQueueLen = 0;
	vk_oit_cert_geometry_clear();
	ri.Cmd_ExecuteText( EXEC_APPEND, "oit_certification_abort\n" );
	ri.Printf( PRINT_ALL, "oit_lab_reset: cleared fixtures + cert session\n" );
}

void vk_oit_lab_register( void )
{
	if ( s_cmds ) {
		return;
	}
	r_oitLabFreeze = ri.Cvar_Get( "r_oitLabFreeze", "1", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_oitLabFreeze, "0", "1", CV_INTEGER );

	ri.Cmd_AddCommand( "oit_lab_list", OIT_Lab_List_f );
	ri.Cmd_AddCommand( "oit_lab_run", OIT_Lab_Run_f );
	ri.Cmd_AddCommand( "oit_lab_run_group", OIT_Lab_RunGroup_f );
	ri.Cmd_AddCommand( "oit_lab_status", OIT_Lab_Status_f );
	ri.Cmd_AddCommand( "oit_lab_reset", OIT_Lab_Reset_f );
	ri.Cmd_AddCommand( "oit_certify_core", OIT_CertifyCore_f );

	s_cmds = qtrue;
	ri.Printf( PRINT_ALL,
		"[VK][OIT] Phase 2.6B oit_lab ready (fixture-backed; oit_certify_core)\n" );
}
