/*
===========================================================================
Phase 2.6C — live certification execution, deferred GPU snapshots, failure triage.

Policy: WBOIT_EVIDENCE_CPU_REFERENCE must never be recorded as a silent GPU PASS.
Missing GPU readback → PENDING / WBOIT_EVIDENCE_NONE, not a forged PASS.
Evaluate only from finalized OIT snapshots (never mid-frame unsubmitted images).
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
#include "vk_oit_certify.h"

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
	OIT_LAB_EVAL_LIFECYCLE,
	OIT_LAB_EVAL_MBOIT_SINGLE
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
static qboolean s_evalAwaitingSnapshot;
static int s_pendingRetries;
static cvar_t *r_oitCertIsolate;
static cvar_t *r_oitCertContinueOnFail;
static cvar_t *r_oitCertSoakMinutes;
static cvar_t *r_oitCertMaxRetries;
static wboitCertStatus_t s_lastRecordedStatus;
static certMetrics_t s_lastMboitImageDiff;
static char s_lastMboitStatus[32];
static char s_lastMboitNotes[192];

static void OIT_Lab_AdvanceCoreQueue( void );

static void OIT_Lab_ApplyFreeze( void )
{
	if ( r_oitLabFreeze && r_oitLabFreeze->integer ) {
		ri.Cvar_Set( "r_transparencyFreeze", "1" );
		ri.Cvar_Set( "r_taa", "0" );
	}
	if ( r_oitCertIsolate && r_oitCertIsolate->integer ) {
		ri.Cvar_Set( "r_oitLabFreeze", "1" );
	}
}

qboolean vk_oit_lab_isolate_world( void )
{
	return ( r_oitCertIsolate && r_oitCertIsolate->integer &&
		( s_pendingEval != OIT_LAB_EVAL_NONE || s_evalAwaitingSnapshot || s_coreRunning ) ) ? qtrue : qfalse;
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
	oitCertScenario_t sc;
	const float color[3] = { 0.25f, 0.6f, 0.95f };
	ri.Cvar_Set( "r_oit", "2" );
	ri.Cvar_Set( "r_oitForwardPlus", "1" );
	vk_oit_cert_geometry_make_single_layer( &sc, 0.42f, color, 256.0f );
	Q_strncpyz( sc.name, "mboit_single_layer_diff", sizeof( sc.name ) );
	vk_oit_cert_geometry_arm( &sc );
	OIT_Lab_ArmEval( OIT_LAB_EVAL_MBOIT_SINGLE, WBOIT_CERT_STAGE_COUNT, "mboit_single_layer_diff" );
	ri.Printf( PRINT_ALL, "oit_lab mboit: armed live image-diff single-layer case (r_oit 2)\n" );
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
	s_lastRecordedStatus = status;
	Q_strncpyz( s_lastStatus,
		( status == WBOIT_CERT_STATUS_PASS ) ? "PASS" :
		( status == WBOIT_CERT_STATUS_FAIL ) ? "FAIL" : "PENDING",
		sizeof( s_lastStatus ) );
	if ( status == WBOIT_CERT_STATUS_FAIL ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
			"[VK][OIT-cert] FAIL %s stage=%s: %s\n" S_COLOR_WHITE,
			s_pendingTest, vk_wboit_cert_stage_name( s_pendingStage ),
			reason ? reason : "" );
	} else if ( status == WBOIT_CERT_STATUS_PASS ) {
		ri.Printf( PRINT_ALL, "[VK][OIT-cert] PASS %s evidence=%s\n",
			s_pendingTest, vk_wboit_cert_evidence_name( evidence ) );
	}
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

static qboolean OIT_Lab_GetSnapshot( const certOitSnapshot_t **out )
{
	const certOitSnapshot_t *snap = vk_cert_readback_last_oit_snapshot();
	if ( !snap || !snap->valid || !snap->fog.valid || !snap->accum.valid ||
		!snap->reveal.valid || !snap->resolved.valid ) {
		return qfalse;
	}
	*out = snap;
	return qtrue;
}

static qboolean OIT_Lab_BuildSingleLayerReference( const certOitSnapshot_t *snap,
	const oitCertScenario_t *sc, float **refOut, uint8_t **maskOut, uint32_t *maskCountOut )
{
	uint32_t i, n, maskCount = 0;
	float *ref;
	uint8_t *mask;
	if ( !snap || !sc || !snap->fog.rgba || !snap->accum.rgba || !snap->reveal.rgba ||
		!snap->resolved.rgba || snap->fog.width != snap->resolved.width ||
		snap->fog.height != snap->resolved.height ) {
		return qfalse;
	}
	n = snap->fog.width * snap->fog.height;
	ref = (float *)malloc( sizeof( float ) * n * 4 );
	mask = (uint8_t *)malloc( sizeof( uint8_t ) * n );
	if ( !ref || !mask ) {
		free( ref );
		free( mask );
		return qfalse;
	}
	for ( i = 0; i < n; i++ ) {
		const float *fog = snap->fog.rgba + i * 4;
		float *dst = ref + i * 4;
		float accumWeight = snap->accum.rgba[i * 4 + 3];
		float reveal = snap->reveal.rgba[i * 4];
		mask[i] = ( accumWeight > 1e-5f || reveal < 0.999f ) ? 1u : 0u;
		if ( mask[i] ) {
			vk_oit_cert_geometry_expect_source_over( sc->expectSingleColor,
				sc->expectSingleOpacity, fog, dst );
			dst[3] = 1.0f;
			maskCount++;
		} else {
			dst[0] = fog[0];
			dst[1] = fog[1];
			dst[2] = fog[2];
			dst[3] = fog[3];
		}
	}
	*refOut = ref;
	*maskOut = mask;
	if ( maskCountOut ) {
		*maskCountOut = maskCount;
	}
	return qtrue;
}

static void OIT_Lab_EvalEmpty( void )
{
	const certOitSnapshot_t *snap;
	certMetrics_t m;
	float *reveal1;
	uint32_t i, n;

	if ( !OIT_Lab_GetSnapshot( &snap ) ) {
		OIT_Lab_RecordPending( WBOIT_CERT_STATUS_PENDING, WBOIT_EVIDENCE_NONE, 0, 0,
			"OIT snapshot unavailable" );
		return;
	}
	n = snap->fog.width * snap->fog.height;
	reveal1 = (float *)malloc( sizeof( float ) * n );
	if ( !reveal1 ) {
		return;
	}
	for ( i = 0; i < n; i++ ) {
		reveal1[i] = snap->reveal.rgba[i * 4];
	}
	vk_cert_metrics_empty_pixels( snap->fog.rgba, snap->accum.rgba, reveal1, snap->resolved.rgba,
		snap->fog.width, snap->fog.height, 1e-3f, &m );
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
	const certOitSnapshot_t *snap;
	const oitCertScenario_t *sc = vk_oit_cert_geometry_scenario();
	float fogC[4], resC[4], expect[3];
	float err;
	float *ref = NULL;
	uint8_t *mask = NULL;
	uint32_t maskCount = 0;
	certMetrics_t diff;

	if ( !sc || !OIT_Lab_GetSnapshot( &snap ) ) {
		OIT_Lab_RecordPending( WBOIT_CERT_STATUS_PENDING, WBOIT_EVIDENCE_NONE, 0, 2e-2,
			"single-layer snapshot unavailable" );
		return;
	}
	if ( !OIT_Lab_CenterSample( snap->fog.rgba, snap->fog.width, snap->fog.height, fogC ) ||
		!OIT_Lab_CenterSample( snap->resolved.rgba, snap->resolved.width, snap->resolved.height, resC ) ) {
		OIT_Lab_RecordPending( WBOIT_CERT_STATUS_FAIL, WBOIT_EVIDENCE_GPU_READBACK, 1, 2e-2,
			"center sample failed" );
		return;
	}
	vk_oit_cert_geometry_expect_source_over( sc->expectSingleColor, sc->expectSingleOpacity, fogC, expect );
	err = fmaxf( fabsf( resC[0] - expect[0] ), fmaxf( fabsf( resC[1] - expect[1] ), fabsf( resC[2] - expect[2] ) ) );
	vk_cert_metrics_clear( &diff );
	if ( !OIT_Lab_BuildSingleLayerReference( snap, sc, &ref, &mask, &maskCount ) || maskCount == 0 ) {
		free( ref );
		free( mask );
		if ( vk_oit_cert_geometry_was_drawn() ) {
			OIT_Lab_RecordPending( WBOIT_CERT_STATUS_FAIL, WBOIT_EVIDENCE_GPU_READBACK, err, 2e-2,
				"fixture submitted but accum/reveal snapshot has no covered pixels" );
		} else {
			OIT_Lab_RecordPending( WBOIT_CERT_STATUS_PENDING, WBOIT_EVIDENCE_NONE, err, 2e-2,
				"single-layer fixture was not drawn" );
		}
		return;
	}
	vk_cert_metrics_compare_rgba( snap->resolved.rgba, ref, snap->resolved.width, snap->resolved.height,
		mask, &diff );
	free( ref );
	free( mask );
	{
		char notes[192];
		Com_sprintf( notes, sizeof( notes ),
			"imageDiff rmse=%g maxAbs=%g meanRelLum=%g pixels=%u centerAbs=%g",
			diff.rmse, diff.maxAbsRgb, diff.meanRelLum, diff.validPixelCount, err );
		if ( err <= 5e-2f &&
			vk_cert_metrics_image_diff_passes( &diff, 3.5e-2, 8.0e-2, 8.0e-2 ) ) {
			OIT_Lab_RecordPending( WBOIT_CERT_STATUS_PASS, WBOIT_EVIDENCE_GPU_IMAGE_DIFF,
				diff.rmse, 3.5e-2, notes );
		} else {
			OIT_Lab_RecordPending( WBOIT_CERT_STATUS_FAIL, WBOIT_EVIDENCE_GPU_IMAGE_DIFF,
				diff.rmse, 3.5e-2, notes );
		}
	}
}

static void OIT_Lab_EvalMboitSingle( void )
{
	const certOitSnapshot_t *snap;
	const oitCertScenario_t *sc = vk_oit_cert_geometry_scenario();
	float *ref = NULL;
	uint8_t *mask = NULL;
	uint32_t maskCount = 0;
	certMetrics_t diff;
	if ( !sc || !OIT_Lab_GetSnapshot( &snap ) ) {
		Q_strncpyz( s_lastMboitStatus, "PENDING", sizeof( s_lastMboitStatus ) );
		Q_strncpyz( s_lastMboitNotes, "MBOIT image-diff snapshot unavailable", sizeof( s_lastMboitNotes ) );
		s_lastRecordedStatus = WBOIT_CERT_STATUS_PENDING;
		return;
	}
	vk_cert_metrics_clear( &diff );
	if ( !OIT_Lab_BuildSingleLayerReference( snap, sc, &ref, &mask, &maskCount ) || maskCount == 0 ) {
		free( ref );
		free( mask );
		Q_strncpyz( s_lastMboitStatus, "PENDING", sizeof( s_lastMboitStatus ) );
		Q_strncpyz( s_lastMboitNotes, "MBOIT image-diff reference unavailable", sizeof( s_lastMboitNotes ) );
		s_lastRecordedStatus = WBOIT_CERT_STATUS_PENDING;
		return;
	}
	vk_cert_metrics_compare_rgba( snap->resolved.rgba, ref, snap->resolved.width, snap->resolved.height,
		mask, &diff );
	free( ref );
	free( mask );
	s_lastMboitImageDiff = diff;
	Com_sprintf( s_lastMboitNotes, sizeof( s_lastMboitNotes ),
		"rmse=%g maxAbs=%g meanRelLum=%g pixels=%u", diff.rmse, diff.maxAbsRgb,
		diff.meanRelLum, diff.validPixelCount );
	if ( vk_cert_metrics_image_diff_passes( &diff, 4.5e-2, 9.0e-2, 1.0e-1 ) ) {
		Q_strncpyz( s_lastMboitStatus, "PASS", sizeof( s_lastMboitStatus ) );
		s_lastRecordedStatus = WBOIT_CERT_STATUS_PASS;
		ri.Printf( PRINT_ALL, "[VK][MBOIT-cert] PASS image-diff %s\n", s_lastMboitNotes );
	} else {
		Q_strncpyz( s_lastMboitStatus, "FAIL", sizeof( s_lastMboitStatus ) );
		s_lastRecordedStatus = WBOIT_CERT_STATUS_FAIL;
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW "[VK][MBOIT-cert] FAIL image-diff %s\n" S_COLOR_WHITE,
			s_lastMboitNotes );
	}
}

static void OIT_Lab_EvalRevealage( void )
{
	const certOitSnapshot_t *snap;
	const oitCertScenario_t *sc = vk_oit_cert_geometry_scenario();
	float sample[4];
	float err;

	if ( !sc || !OIT_Lab_GetSnapshot( &snap ) ) {
		OIT_Lab_RecordPending( WBOIT_CERT_STATUS_PENDING, WBOIT_EVIDENCE_NONE, 0, 5e-2,
			"revealage snapshot unavailable" );
		return;
	}
	if ( !OIT_Lab_CenterSample( snap->reveal.rgba, snap->reveal.width, snap->reveal.height, sample ) ) {
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
	const certOitSnapshot_t *snap;
	const oitWeightContract_t *w = vk_oit_weight_contract_get();
	certMetrics_t m;
	float *weights;
	uint32_t i, n, k;

	if ( !w || !OIT_Lab_GetSnapshot( &snap ) ) {
		OIT_Lab_RecordPending( WBOIT_CERT_STATUS_PENDING, WBOIT_EVIDENCE_NONE, 0, 0,
			"weight snapshot unavailable" );
		return;
	}
	n = snap->accum.pixelCount;
	weights = (float *)malloc( sizeof( float ) * n );
	if ( !weights ) {
		return;
	}
	k = 0;
	for ( i = 0; i < n; i++ ) {
		float wt = snap->accum.rgba[i * 4 + 3];
		if ( wt > 1e-6f ) {
			weights[k++] = wt;
		}
	}
	if ( k == 0 ) {
		free( weights );
		if ( vk_oit_cert_geometry_was_drawn() ) {
			OIT_Lab_RecordPending( WBOIT_CERT_STATUS_FAIL, WBOIT_EVIDENCE_GPU_REDUCTION, 0, 0,
				"fixture submitted but accumulation weight stayed empty" );
		} else {
			OIT_Lab_RecordPending( WBOIT_CERT_STATUS_PENDING, WBOIT_EVIDENCE_NONE, 0, 0,
				"no weighted fragments — fixture was not drawn" );
		}
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
	const certOitSnapshot_t *snap;
	float sample[4];
	float lum;

	if ( !OIT_Lab_GetSnapshot( &snap ) ||
		!OIT_Lab_CenterSample( snap->resolved.rgba, snap->resolved.width, snap->resolved.height, sample ) ) {
		OIT_Lab_RecordPending( WBOIT_CERT_STATUS_PENDING, WBOIT_EVIDENCE_NONE, 0, 0.05,
			"order snapshot unavailable" );
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
		s_evalAwaitingSnapshot = qfalse;
		Q_strncpyz( s_lastStatus, "ARMED_NEXT_PERM", sizeof( s_lastStatus ) );
		ri.Printf( PRINT_ALL, "oit_lab order: captured perm %d lum=%g — arming next\n",
			s_orderPerm - 1, lum );
		s_lastRecordedStatus = WBOIT_CERT_STATUS_PENDING;
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
	const certOitSnapshot_t *snap;
	float a[4], b[4];
	Com_Memset( a, 0, sizeof( a ) );
	Com_Memset( b, 0, sizeof( b ) );
	if ( !OIT_Lab_GetSnapshot( &snap ) ) {
		OIT_Lab_RecordPending( WBOIT_CERT_STATUS_PENDING, WBOIT_EVIDENCE_NONE, 0, 0,
			"fog snapshot unavailable" );
		return;
	}
	if ( !OIT_Lab_CenterSample( snap->fog.rgba, snap->fog.width, snap->fog.height, a ) ||
		!OIT_Lab_CenterSample( snap->resolved.rgba, snap->resolved.width, snap->resolved.height, b ) ) {
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
	const certOitSnapshot_t *snap;
	const oitCertScenario_t *sc = vk_oit_cert_geometry_scenario();
	float sample[4];
	float err;

	if ( !sc || !OIT_Lab_GetSnapshot( &snap ) ||
		!OIT_Lab_CenterSample( snap->reveal.rgba, snap->reveal.width, snap->reveal.height, sample ) ) {
		OIT_Lab_RecordPending( WBOIT_CERT_STATUS_PENDING, WBOIT_EVIDENCE_NONE, 0, 0,
			"additive revealage snapshot unavailable" );
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
	const certOitSnapshot_t *snap;
	if ( !OIT_Lab_GetSnapshot( &snap ) ||
		snap->fog.generation == 0 || snap->fog.generation != snap->resolved.generation ) {
		OIT_Lab_RecordPending( WBOIT_CERT_STATUS_PENDING, WBOIT_EVIDENCE_NONE, 0, 0,
			"hdr resolve generation mismatch or unavailable" );
		return;
	}
	{
		char notes[128];
		Com_sprintf( notes, sizeof( notes ), "fog/resolved gen=%u frame=%llu",
			snap->fog.generation, (unsigned long long)snap->fog.frameNumber );
		OIT_Lab_RecordPending( WBOIT_CERT_STATUS_PASS, WBOIT_EVIDENCE_GPU_READBACK,
			(double)snap->fog.generation, 0, notes );
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
	if ( s_pendingEval == OIT_LAB_EVAL_NONE ) {
		return;
	}
	if ( !vk.cmd || !vk.cmd->command_buffer ) {
		return;
	}
	if ( !vk_cert_readback_record_oit_snapshot( vk.cmd->command_buffer, vk.cmd_index ) ) {
		ri.Printf( PRINT_WARNING, "[VK][OIT-cert] snapshot record failed — will retry\n" );
		return;
	}
	s_evalAwaitingSnapshot = qtrue;
}

static void OIT_Lab_TriageAdvance( void )
{
	int maxRetries = r_oitCertMaxRetries ? r_oitCertMaxRetries->integer : 8;

	if ( s_lastRecordedStatus == WBOIT_CERT_STATUS_PASS ) {
		s_pendingRetries = 0;
		s_pendingEval = OIT_LAB_EVAL_NONE;
		OIT_Lab_AdvanceCoreQueue();
		return;
	}
	if ( s_lastRecordedStatus == WBOIT_CERT_STATUS_FAIL ) {
		s_pendingEval = OIT_LAB_EVAL_NONE;
		if ( r_oitCertContinueOnFail && r_oitCertContinueOnFail->integer ) {
			ri.Printf( PRINT_WARNING, "[VK][OIT-cert] continuing after FAIL (r_oitCertContinueOnFail)\n" );
			OIT_Lab_AdvanceCoreQueue();
		} else {
			s_coreRunning = qfalse;
			ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
				"[VK][OIT-cert] queue stopped on FAIL — fix then oit_certify_core / oit_lab_run\n"
				S_COLOR_WHITE );
			ri.Cmd_ExecuteText( EXEC_APPEND, "wboit_production_status\n" );
		}
		return;
	}
	/* PENDING: retry same armed eval */
	s_pendingRetries++;
	if ( s_pendingRetries > maxRetries ) {
		OIT_Lab_RecordPending( WBOIT_CERT_STATUS_FAIL, WBOIT_EVIDENCE_NONE, (double)s_pendingRetries, 0,
			"exceeded pending retries — need r_oit 1 + in-world camera" );
		s_pendingEval = OIT_LAB_EVAL_NONE;
		s_coreRunning = qfalse;
		return;
	}
	ri.Printf( PRINT_ALL, "[VK][OIT-cert] PENDING retry %d/%d for %s\n",
		s_pendingRetries, maxRetries, s_pendingTest );
}

void vk_oit_lab_finalize_frame( int cmdIndex )
{
	oitLabEval_t eval;
	if ( !s_evalAwaitingSnapshot ) {
		return;
	}
	if ( !vk_cert_readback_finalize_oit_snapshot( cmdIndex, NULL ) ) {
		return;
	}
	s_evalAwaitingSnapshot = qfalse;
	eval = s_pendingEval;
	if ( eval == OIT_LAB_EVAL_NONE ) {
		return;
	}
	if ( !vk_oit_cert_geometry_was_drawn() && eval != OIT_LAB_EVAL_EMPTY &&
		eval != OIT_LAB_EVAL_HDR && eval != OIT_LAB_EVAL_LIFECYCLE &&
		eval != OIT_LAB_EVAL_ORDER ) {
		OIT_Lab_RecordPending( WBOIT_CERT_STATUS_PENDING, WBOIT_EVIDENCE_NONE, 0, 0,
			"fixtures armed but not drawn this OIT frame" );
		OIT_Lab_TriageAdvance();
		return;
	}

	s_lastRecordedStatus = WBOIT_CERT_STATUS_PENDING;
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
	case OIT_LAB_EVAL_MBOIT_SINGLE: OIT_Lab_EvalMboitSingle(); break;
	default: break;
	}

	if ( eval == OIT_LAB_EVAL_ORDER && s_pendingEval == OIT_LAB_EVAL_ORDER &&
		s_lastRecordedStatus == WBOIT_CERT_STATUS_PENDING &&
		!Q_stricmp( s_lastStatus, "ARMED_NEXT_PERM" ) ) {
		return;
	}
	OIT_Lab_TriageAdvance();
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
		int soakMin;
		s_coreRunning = qfalse;
		vk_oit_certify_note_gpu_core_complete();
		ri.Printf( PRINT_ALL,
			"oit_certify_core: GPU queue complete — level=%s\n",
			vk_wboit_production_level_name( vk_wboit_production_level() ) );
		ri.Cmd_ExecuteText( EXEC_APPEND, "oit_certification_export\n" );
		soakMin = r_oitCertSoakMinutes ? r_oitCertSoakMinutes->integer : 1;
		if ( soakMin > 0 ) {
			char cmd[64];
			Com_sprintf( cmd, sizeof( cmd ), "oit_soak_wboit %d\n", soakMin );
			ri.Printf( PRINT_ALL,
				"oit_certify_core: chaining soak %d min for PRODUCTION (r_oitCertSoakMinutes)\n",
				soakMin );
			ri.Cmd_ExecuteText( EXEC_APPEND, cmd );
		}
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

static void OIT_Lab_MboitStatus_f( void )
{
	ri.Printf( PRINT_ALL,
		"mboit_image_diff_status: status=%s rmse=%g maxAbs=%g meanRelLum=%g pixels=%u\n"
		"  notes=%s\n"
		"  command: oit_lab_run mboit_compare\n",
		s_lastMboitStatus[0] ? s_lastMboitStatus : "PENDING",
		s_lastMboitImageDiff.rmse,
		s_lastMboitImageDiff.maxAbsRgb,
		s_lastMboitImageDiff.meanRelLum,
		s_lastMboitImageDiff.validPixelCount,
		s_lastMboitNotes[0] ? s_lastMboitNotes : "-" );
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
	r_oitCertIsolate = ri.Cvar_Get( "r_oitCertIsolate", "1", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_oitCertIsolate, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_oitCertIsolate,
		"Skip world transparent draws while cert fixtures are armed (deterministic GPU evidence)." );
	r_oitCertContinueOnFail = ri.Cvar_Get( "r_oitCertContinueOnFail", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_oitCertContinueOnFail, "0", "1", CV_INTEGER );
	r_oitCertSoakMinutes = ri.Cvar_Get( "r_oitCertSoakMinutes", "1", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_oitCertSoakMinutes, "0", "240", CV_INTEGER );
	ri.Cvar_SetDescription( r_oitCertSoakMinutes,
		"After oit_certify_core GPU queue, auto-run oit_soak_wboit N minutes (0=skip). Formal shipping: 30." );
	r_oitCertMaxRetries = ri.Cvar_Get( "r_oitCertMaxRetries", "8", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_oitCertMaxRetries, "1", "120", CV_INTEGER );

	ri.Cmd_AddCommand( "oit_lab_list", OIT_Lab_List_f );
	ri.Cmd_AddCommand( "oit_lab_run", OIT_Lab_Run_f );
	ri.Cmd_AddCommand( "oit_lab_run_group", OIT_Lab_RunGroup_f );
	ri.Cmd_AddCommand( "oit_lab_status", OIT_Lab_Status_f );
	ri.Cmd_AddCommand( "oit_lab_reset", OIT_Lab_Reset_f );
	ri.Cmd_AddCommand( "oit_certify_core", OIT_CertifyCore_f );
	ri.Cmd_AddCommand( "mboit_image_diff_status", OIT_Lab_MboitStatus_f );

	s_cmds = qtrue;
	ri.Printf( PRINT_ALL,
		"[VK][OIT] Phase 2.6C oit_lab ready (deferred snapshots; oit_certify_core)\n" );
}

void vk_oit_lab_shutdown( void )
{
	if ( !s_cmds ) {
		return;
	}

	/* Renderer commands point into the Vulkan DSO. Remove them before a
	   vid_restart unloads the renderer; otherwise the command table can call
	   stale function pointers on the next exec (notably oit_certify_core). */
	ri.Cmd_RemoveCommand( "oit_lab_list" );
	ri.Cmd_RemoveCommand( "oit_lab_run" );
	ri.Cmd_RemoveCommand( "oit_lab_run_group" );
	ri.Cmd_RemoveCommand( "oit_lab_status" );
	ri.Cmd_RemoveCommand( "oit_lab_reset" );
	ri.Cmd_RemoveCommand( "oit_certify_core" );
	ri.Cmd_RemoveCommand( "mboit_image_diff_status" );

	s_pendingEval = OIT_LAB_EVAL_NONE;
	s_coreRunning = qfalse;
	s_coreQueueLen = 0;
	s_coreQueuePos = 0;
	s_evalAwaitingSnapshot = qfalse;
	s_pendingRetries = 0;
	vk_oit_cert_geometry_clear();
	s_cmds = qfalse;
}
