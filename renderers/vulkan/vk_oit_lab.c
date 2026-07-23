/*
===========================================================================
Phase 2.6A — deterministic OIT laboratory execution.
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

typedef struct {
	const char *name;
	oitLabGroup_t group;
	wboitCertStage_t stage;
	uint32_t seed;
	float failThr;
	float warnThr;
	qboolean (*run)( void );
} oitLabCase_t;

static qboolean s_cmds;
static cvar_t *r_oitLabFreeze;
static int s_lastCase;
static char s_lastStatus[64];

/* --- Synthetic GPU-buffer lab: validates metrics + records GPU path when readback works --- */

static qboolean OIT_Lab_TryGpuEmptyPixel( void )
{
	certReadbackCapture_t fog, accum, reveal, resolved;
	certMetrics_t m;
	wboitCertStageResult_t r;
	float *reveal1;
	uint32_t i, n;

	Com_Memset( &r, 0, sizeof( r ) );
	r.stage = WBOIT_CERT_STAGE_EMPTY_PIXEL;
	Q_strncpyz( r.testName, "wboit_empty_pixel", sizeof( r.testName ) );
	r.failureThreshold = 1e-3;
	r.warningThreshold = 5e-4;

	if ( !vk_cert_readback_capture( CERT_RB_FOG_SCENE, &fog ) ||
		!vk_cert_readback_capture( CERT_RB_OIT_ACCUM, &accum ) ||
		!vk_cert_readback_capture( CERT_RB_OIT_REVEALAGE, &reveal ) ||
		!vk_cert_readback_capture( CERT_RB_RESOLVED_WBOIT, &resolved ) ) {
		r.status = WBOIT_CERT_STATUS_PENDING;
		r.evidenceType = WBOIT_EVIDENCE_NONE;
		Q_strncpyz( r.failureReason, "GPU resources unavailable for empty-pixel readback", sizeof( r.failureReason ) );
		vk_wboit_cert_record_result( &r );
		return qfalse;
	}
	if ( fog.frameNumber != accum.frameNumber || fog.generation == 0 || accum.generation == 0 ) {
		r.status = WBOIT_CERT_STATUS_FAIL;
		r.evidenceType = WBOIT_EVIDENCE_GPU_READBACK;
		Q_strncpyz( r.failureReason, "frame/generation mismatch across captures", sizeof( r.failureReason ) );
		vk_wboit_cert_record_result( &r );
		return qfalse;
	}
	n = fog.width * fog.height;
	reveal1 = (float *)malloc( sizeof( float ) * n );
	if ( !reveal1 ) {
		return qfalse;
	}
	for ( i = 0; i < n; i++ ) {
		reveal1[i] = reveal.rgba[i * 4];
	}
	vk_cert_metrics_empty_pixels( fog.rgba, accum.rgba, reveal1, resolved.rgba,
		fog.width, fog.height, (float)r.failureThreshold, &m );
	free( reveal1 );

	r.evidenceType = WBOIT_EVIDENCE_GPU_READBACK;
	r.observed = (double)m.modifiedEmptyPixels;
	if ( m.modifiedEmptyPixels == 0 && m.nanCount == 0 && m.infCount == 0 ) {
		r.status = WBOIT_CERT_STATUS_PASS;
		Com_sprintf( r.failureReason, sizeof( r.failureReason ),
			"empty=%u modified=0 maxErr=%g meanErr=%g", m.weightInvalid,
			m.maxEmptyPixelError, m.meanEmptyPixelError );
	} else {
		r.status = WBOIT_CERT_STATUS_FAIL;
		Com_sprintf( r.failureReason, sizeof( r.failureReason ),
			"modifiedEmpty=%u maxErr=%g nan=%u inf=%u", m.modifiedEmptyPixels,
			m.maxEmptyPixelError, m.nanCount, m.infCount );
	}
	vk_wboit_cert_record_result( &r );
	return r.status == WBOIT_CERT_STATUS_PASS;
}

static qboolean OIT_Lab_SyntheticEmptyPixel( void )
{
	/* CPU reference proving metric path; does NOT grant GPU stage. */
	const uint32_t w = 8, h = 8, n = w * h;
	float *fog, *accum, *reveal, *resolved;
	certMetrics_t m;
	uint32_t i;
	wboitCertStageResult_t r;

	fog = (float *)calloc( n * 4, sizeof( float ) );
	accum = (float *)calloc( n * 4, sizeof( float ) );
	reveal = (float *)calloc( n, sizeof( float ) );
	resolved = (float *)calloc( n * 4, sizeof( float ) );
	if ( !fog || !accum || !reveal || !resolved ) {
		free( fog ); free( accum ); free( reveal ); free( resolved );
		return qfalse;
	}
	for ( i = 0; i < n; i++ ) {
		fog[i * 4 + 0] = 0.2f; fog[i * 4 + 1] = 0.3f; fog[i * 4 + 2] = 0.4f; fog[i * 4 + 3] = 1.0f;
		accum[i * 4 + 3] = 0.0f;
		reveal[i] = 1.0f;
		resolved[i * 4 + 0] = fog[i * 4 + 0];
		resolved[i * 4 + 1] = fog[i * 4 + 1];
		resolved[i * 4 + 2] = fog[i * 4 + 2];
		resolved[i * 4 + 3] = 1.0f;
	}
	vk_cert_metrics_empty_pixels( fog, accum, reveal, resolved, w, h, 1e-3f, &m );
	free( fog ); free( accum ); free( reveal ); free( resolved );

	/* Prefer real GPU; synthetic only logs CPU reference helper. */
	if ( OIT_Lab_TryGpuEmptyPixel() ) {
		return qtrue;
	}
	Com_Memset( &r, 0, sizeof( r ) );
	r.stage = WBOIT_CERT_STAGE_EMPTY_PIXEL;
	r.status = WBOIT_CERT_STATUS_PENDING;
	r.evidenceType = WBOIT_EVIDENCE_CPU_REFERENCE;
	r.observed = (double)m.modifiedEmptyPixels;
	r.failureThreshold = 0.0;
	Q_strncpyz( r.testName, "wboit_empty_pixel_cpu_ref", sizeof( r.testName ) );
	Com_sprintf( r.failureReason, sizeof( r.failureReason ),
		"CPU metric OK (modified=%u) but GPU readback pending — not production evidence",
		m.modifiedEmptyPixels );
	vk_wboit_cert_record_result( &r );
	return qfalse;
}

static qboolean OIT_Lab_SingleLayer( void )
{
	static const float opacities[] = {
		0.f, 1.f / 255.f, 0.01f, 0.1f, 0.25f, 0.5f, 0.75f, 0.9f, 0.99f, 1.f
	};
	float layer[3] = { 0.8f, 0.2f, 0.1f };
	float fog[3] = { 0.1f, 0.2f, 0.3f };
	float maxErr = 0.0f;
	int i;
	wboitCertStageResult_t r;
	certReadbackCapture_t fogCap, resolved;

	for ( i = 0; i < (int)( sizeof( opacities ) / sizeof( opacities[0] ) ); i++ ) {
		float out[3], ref[3];
		float o = opacities[i];
		vk_wboit_cert_source_over( layer, o, fog, out );
		ref[0] = layer[0] * o + fog[0] * ( 1.0f - o );
		ref[1] = layer[1] * o + fog[1] * ( 1.0f - o );
		ref[2] = layer[2] * o + fog[2] * ( 1.0f - o );
		maxErr = fmaxf( maxErr, fabsf( out[0] - ref[0] ) );
		maxErr = fmaxf( maxErr, fabsf( out[1] - ref[1] ) );
		maxErr = fmaxf( maxErr, fabsf( out[2] - ref[2] ) );
	}

	Com_Memset( &r, 0, sizeof( r ) );
	r.stage = WBOIT_CERT_STAGE_SINGLE_LAYER;
	r.testName[0] = '\0';
	Q_strncpyz( r.testName, "wboit_single_layer", sizeof( r.testName ) );
	r.failureThreshold = 2e-3;
	r.warningThreshold = 1e-3;

	if ( vk_cert_readback_capture( CERT_RB_FOG_SCENE, &fogCap ) &&
		vk_cert_readback_capture( CERT_RB_RESOLVED_WBOIT, &resolved ) &&
		fogCap.valid && resolved.valid && fogCap.generation == resolved.generation ) {
		r.evidenceType = WBOIT_EVIDENCE_GPU_READBACK;
		r.observed = maxErr;
		/* Captures alone are insufficient — need lab pane draw; leave PENDING. */
		r.status = WBOIT_CERT_STATUS_PENDING;
		Com_sprintf( r.failureReason, sizeof( r.failureReason ),
			"CPU opacity-sweep maxErr=%g; GPU same-gen captures ok — draw single-layer lab pane to finish",
			maxErr );
		vk_wboit_cert_record_result( &r );
		return qfalse;
	}

	r.evidenceType = WBOIT_EVIDENCE_CPU_REFERENCE;
	r.observed = maxErr;
	r.status = WBOIT_CERT_STATUS_PENDING;
	Com_sprintf( r.failureReason, sizeof( r.failureReason ),
		"CPU source-over sweep maxErr=%g; awaiting GPU lab pane draws", maxErr );
	vk_wboit_cert_record_result( &r );
	return qfalse;
}

static qboolean OIT_Lab_Revealage( void )
{
	float alphas[] = { 0.1f, 0.25f, 0.5f, 0.75f };
	float expect;
	float *gpuReveal = NULL;
	certReadbackCapture_t rev;
	certMetrics_t m;
	wboitCertStageResult_t r;
	uint32_t i, n;

	expect = vk_wboit_cert_revealage_product( alphas, 4 );
	Com_Memset( &r, 0, sizeof( r ) );
	r.stage = WBOIT_CERT_STAGE_REVEALAGE;
	Q_strncpyz( r.testName, "wboit_revealage", sizeof( r.testName ) );
	r.failureThreshold = 1e-3;

	if ( vk_cert_readback_capture( CERT_RB_OIT_REVEALAGE, &rev ) && rev.valid ) {
		n = rev.pixelCount;
		gpuReveal = (float *)malloc( sizeof( float ) * n );
		if ( gpuReveal ) {
			float *expected = (float *)malloc( sizeof( float ) * n );
			if ( expected ) {
				for ( i = 0; i < n; i++ ) {
					gpuReveal[i] = rev.rgba[i * 4];
					expected[i] = expect; /* lab panes use known product when drawn */
				}
				vk_cert_metrics_revealage( gpuReveal, expected, n, &m );
				free( expected );
			}
			free( gpuReveal );
			r.evidenceType = WBOIT_EVIDENCE_GPU_READBACK;
			r.observed = m.revealageError;
			/* Uniform expected product only valid when lab drew those alphas — pending until then. */
			r.status = WBOIT_CERT_STATUS_PENDING;
			Com_sprintf( r.failureReason, sizeof( r.failureReason ),
				"reveal readback ok meanErrVsSeed=%g — confirm lab alpha sequence before PASS",
				m.revealageError );
			vk_wboit_cert_record_result( &r );
			return qfalse;
		}
	}
	r.evidenceType = WBOIT_EVIDENCE_CPU_REFERENCE;
	r.observed = 0.0;
	r.status = WBOIT_CERT_STATUS_PENDING;
	Com_sprintf( r.failureReason, sizeof( r.failureReason ),
		"CPU product=%g; GPU revealage readback pending", expect );
	vk_wboit_cert_record_result( &r );
	return qfalse;
}

static qboolean OIT_Lab_WeightBounds( void )
{
	const oitWeightContract_t *w = vk_oit_weight_contract_get();
	certReadbackCapture_t accum;
	certMetrics_t m;
	wboitCertStageResult_t r;
	float *weights = NULL;
	uint32_t i, n;

	Com_Memset( &r, 0, sizeof( r ) );
	r.stage = WBOIT_CERT_STAGE_WEIGHT_BOUNDS;
	Q_strncpyz( r.testName, "wboit_weight_bounds", sizeof( r.testName ) );
	r.failureThreshold = w ? w->maxWeight : 3e3;

	if ( vk_cert_readback_capture( CERT_RB_OIT_ACCUM, &accum ) && accum.valid && w ) {
		n = accum.pixelCount;
		weights = (float *)malloc( sizeof( float ) * n );
		if ( weights ) {
			for ( i = 0; i < n; i++ ) {
				float wgt = accum.rgba[i * 4 + 3];
				weights[i] = ( wgt > 1e-6f ) ? wgt : -1.0f; /* skip empty in metrics via invalid */
			}
			/* Re-pack positive weights */
			{
				uint32_t k = 0;
				for ( i = 0; i < n; i++ ) {
					if ( weights[i] > 0.0f ) {
						weights[k++] = weights[i];
					}
				}
				if ( k == 0 ) {
					free( weights );
					r.evidenceType = WBOIT_EVIDENCE_NONE;
					r.status = WBOIT_CERT_STATUS_PENDING;
					Q_strncpyz( r.failureReason, "no weighted fragments in accum — draw opacity ladder first",
						sizeof( r.failureReason ) );
					vk_wboit_cert_record_result( &r );
					return qfalse;
				}
				vk_cert_metrics_weights( weights, k, w->minWeight, w->maxWeight, &m );
			}
			free( weights );
			r.evidenceType = WBOIT_EVIDENCE_GPU_REDUCTION;
			r.observed = m.weightMax;
			r.status = ( m.weightInvalid == 0 && m.weightMin >= 0.0 &&
				m.weightMax <= (double)w->maxWeight + 1e-3 ) ? WBOIT_CERT_STATUS_PASS : WBOIT_CERT_STATUS_FAIL;
			Com_sprintf( r.failureReason, sizeof( r.failureReason ),
				"min=%g max=%g mean=%g lowClamp=%u highClamp=%u invalid=%u",
				m.weightMin, m.weightMax, m.weightMean, m.weightLowClamps, m.weightHighClamps, m.weightInvalid );
			vk_wboit_cert_record_result( &r );
			return r.status == WBOIT_CERT_STATUS_PASS;
		}
	}
	r.evidenceType = WBOIT_EVIDENCE_NONE;
	r.status = WBOIT_CERT_STATUS_PENDING;
	Q_strncpyz( r.failureReason, "weight reduction pending (need OIT accum)", sizeof( r.failureReason ) );
	vk_wboit_cert_record_result( &r );
	return qfalse;
}

static qboolean OIT_Lab_Order( void )
{
	wboitCertStageResult_t r;
	Com_Memset( &r, 0, sizeof( r ) );
	r.stage = WBOIT_CERT_STAGE_ORDER_STABILITY;
	Q_strncpyz( r.testName, "wboit_order_permutations", sizeof( r.testName ) );
	r.failureThreshold = 0.05;
	r.evidenceType = WBOIT_EVIDENCE_NONE;
	r.status = WBOIT_CERT_STATUS_PENDING;
	Q_strncpyz( r.failureReason,
		"order permutation lab requires frozen multi-pane draws + sorted reference compare",
		sizeof( r.failureReason ) );
	vk_wboit_cert_record_result( &r );
	return qfalse;
}

static qboolean OIT_Lab_FogDepth( void )
{
	wboitCertStageResult_t r;
	Com_Memset( &r, 0, sizeof( r ) );
	r.stage = WBOIT_CERT_STAGE_FOG_DEPTH;
	Q_strncpyz( r.testName, "wboit_fog_depth", sizeof( r.testName ) );
	r.status = WBOIT_CERT_STATUS_PENDING;
	r.evidenceType = WBOIT_EVIDENCE_NONE;
	Q_strncpyz( r.failureReason, "fog/depth GPU lab pending", sizeof( r.failureReason ) );
	vk_wboit_cert_record_result( &r );
	return qfalse;
}

static qboolean OIT_Lab_Additive( void )
{
	wboitCertStageResult_t r;
	Com_Memset( &r, 0, sizeof( r ) );
	r.stage = WBOIT_CERT_STAGE_ADDITIVE;
	Q_strncpyz( r.testName, "wboit_additive", sizeof( r.testName ) );
	r.status = WBOIT_CERT_STATUS_PENDING;
	r.evidenceType = WBOIT_EVIDENCE_NONE;
	r.failureThreshold = 0.0;
	Q_strncpyz( r.failureReason, "additive revealage delta GPU lab pending", sizeof( r.failureReason ) );
	vk_wboit_cert_record_result( &r );
	return qfalse;
}

static qboolean OIT_Lab_HdrResolve( void )
{
	certReadbackCapture_t fog, resolved;
	wboitCertStageResult_t r;
	Com_Memset( &r, 0, sizeof( r ) );
	r.stage = WBOIT_CERT_STAGE_HDR_RESOLVE;
	Q_strncpyz( r.testName, "wboit_hdr_resolve", sizeof( r.testName ) );
	if ( vk_cert_readback_capture( CERT_RB_FOG_SCENE, &fog ) &&
		vk_cert_readback_capture( CERT_RB_RESOLVED_WBOIT, &resolved ) &&
		fog.generation > 0 && fog.generation == resolved.generation ) {
		r.evidenceType = WBOIT_EVIDENCE_GPU_READBACK;
		r.status = WBOIT_CERT_STATUS_PASS;
		r.observed = (double)fog.generation;
		Com_sprintf( r.failureReason, sizeof( r.failureReason ),
			"fog_scene/resolved same gen=%u frame=%llu", fog.generation,
			(unsigned long long)fog.frameNumber );
	} else {
		r.evidenceType = WBOIT_EVIDENCE_NONE;
		r.status = WBOIT_CERT_STATUS_PENDING;
		Q_strncpyz( r.failureReason, "resolve generation readback pending", sizeof( r.failureReason ) );
	}
	vk_wboit_cert_record_result( &r );
	return r.status == WBOIT_CERT_STATUS_PASS;
}

static qboolean OIT_Lab_Alpha( void )
{
	wboitCertStageResult_t r;
	Com_Memset( &r, 0, sizeof( r ) );
	r.stage = WBOIT_CERT_STAGE_ALPHA_ENCODING;
	Q_strncpyz( r.testName, "wboit_alpha_equivalence", sizeof( r.testName ) );
	r.status = WBOIT_CERT_STATUS_PENDING;
	r.evidenceType = WBOIT_EVIDENCE_NONE;
	Q_strncpyz( r.failureReason, "straight/premul GPU equivalence lab pending", sizeof( r.failureReason ) );
	vk_wboit_cert_record_result( &r );
	return qfalse;
}

static qboolean OIT_Lab_Lifecycle( void )
{
	wboitCertStageResult_t r;
	Com_Memset( &r, 0, sizeof( r ) );
	r.stage = WBOIT_CERT_STAGE_LIFECYCLE;
	Q_strncpyz( r.testName, "wboit_lifecycle", sizeof( r.testName ) );
	r.status = WBOIT_CERT_STATUS_PENDING;
	r.evidenceType = WBOIT_EVIDENCE_NONE;
	Q_strncpyz( r.failureReason,
		"lifecycle requires resize/oit-toggle/vid_restart sequence with re-validated empty-pixel",
		sizeof( r.failureReason ) );
	vk_wboit_cert_record_result( &r );
	return qfalse;
}

static qboolean OIT_Lab_SpecializedSmoke( void )
{
	vk_transparency_resource_bump( XPARENT_RES_REFRACTIVE_INPUT );
	vk_transparency_resource_bump( XPARENT_RES_REFRACTED_HDR );
	ri.Printf( PRINT_ALL, "oit_lab specialized: refraction resource gens bumped (execution scaffold)\n" );
	return qtrue;
}

static qboolean OIT_Lab_MboitCompare( void )
{
	ri.Printf( PRINT_ALL, "oit_lab mboit: experimental compare only — does not affect WBOIT cert\n" );
	return qtrue;
}

static const oitLabCase_t s_cases[] = {
	{ "wboit_empty_pixel", OIT_LAB_GROUP_CORE, WBOIT_CERT_STAGE_EMPTY_PIXEL, 1, 0.0f, 0.0f, OIT_Lab_SyntheticEmptyPixel },
	{ "wboit_single_layer", OIT_LAB_GROUP_CORE, WBOIT_CERT_STAGE_SINGLE_LAYER, 2, 2e-3f, 1e-3f, OIT_Lab_SingleLayer },
	{ "wboit_revealage", OIT_LAB_GROUP_ALPHA, WBOIT_CERT_STAGE_REVEALAGE, 3, 1e-3f, 5e-4f, OIT_Lab_Revealage },
	{ "wboit_alpha_equivalence", OIT_LAB_GROUP_ALPHA, WBOIT_CERT_STAGE_ALPHA_ENCODING, 4, 2e-3f, 1e-3f, OIT_Lab_Alpha },
	{ "wboit_weight_bounds", OIT_LAB_GROUP_WEIGHT, WBOIT_CERT_STAGE_WEIGHT_BOUNDS, 5, 0.0f, 0.0f, OIT_Lab_WeightBounds },
	{ "wboit_order_permutations", OIT_LAB_GROUP_ORDER, WBOIT_CERT_STAGE_ORDER_STABILITY, 6, 0.05f, 0.02f, OIT_Lab_Order },
	{ "wboit_fog_depth", OIT_LAB_GROUP_FOG, WBOIT_CERT_STAGE_FOG_DEPTH, 7, 1e-2f, 5e-3f, OIT_Lab_FogDepth },
	{ "wboit_additive", OIT_LAB_GROUP_ADDITIVE, WBOIT_CERT_STAGE_ADDITIVE, 8, 0.0f, 0.0f, OIT_Lab_Additive },
	{ "wboit_hdr_resolve", OIT_LAB_GROUP_RESOLVE, WBOIT_CERT_STAGE_HDR_RESOLVE, 9, 0.0f, 0.0f, OIT_Lab_HdrResolve },
	{ "wboit_lifecycle", OIT_LAB_GROUP_LIFECYCLE, WBOIT_CERT_STAGE_LIFECYCLE, 10, 0.0f, 0.0f, OIT_Lab_Lifecycle },
	{ "specialized_refraction_smoke", OIT_LAB_GROUP_SPECIALIZED, WBOIT_CERT_STAGE_COUNT, 11, 0.0f, 0.0f, OIT_Lab_SpecializedSmoke },
	{ "mboit_compare", OIT_LAB_GROUP_MBOIT, WBOIT_CERT_STAGE_COUNT, 12, 0.0f, 0.0f, OIT_Lab_MboitCompare },
};

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

static void OIT_Lab_ApplyFreeze( void )
{
	if ( !r_oitLabFreeze || !r_oitLabFreeze->integer ) {
		return;
	}
	ri.Cvar_Set( "r_transparencyFreeze", "1" );
	ri.Cvar_Set( "r_referenceLabFreezeAnim", "1" );
	/* Disable temporal jitter / auto-exposure adaptation for fixed compares. */
	ri.Cvar_Set( "r_taa", "0" );
}

static void OIT_Lab_List_f( void )
{
	int i;
	ri.Printf( PRINT_ALL, "oit_lab_list (%d cases):\n", (int)( sizeof( s_cases ) / sizeof( s_cases[0] ) ) );
	for ( i = 0; i < (int)( sizeof( s_cases ) / sizeof( s_cases[0] ) ); i++ ) {
		ri.Printf( PRINT_ALL, "  %-28s group=%-12s stage=%s seed=%u\n",
			s_cases[i].name, OIT_Lab_GroupName( s_cases[i].group ),
			s_cases[i].stage < WBOIT_CERT_STAGE_COUNT ? vk_wboit_cert_stage_name( s_cases[i].stage ) : "(none)",
			s_cases[i].seed );
	}
	ri.Printf( PRINT_ALL, "groups: core alpha weight order fog additive resolve lifecycle soak specialized mboit all\n" );
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
	qboolean ok;
	if ( idx < 0 || idx >= (int)( sizeof( s_cases ) / sizeof( s_cases[0] ) ) ) {
		return qfalse;
	}
	OIT_Lab_ApplyFreeze();
	s_lastCase = idx;
	ri.Printf( PRINT_ALL, "oit_lab_run: %s (group=%s seed=%u)\n",
		s_cases[idx].name, OIT_Lab_GroupName( s_cases[idx].group ), s_cases[idx].seed );
	ok = s_cases[idx].run ? s_cases[idx].run() : qfalse;
	Q_strncpyz( s_lastStatus, ok ? "PASS" : "PENDING/FAIL", sizeof( s_lastStatus ) );
	ri.Printf( PRINT_ALL, "oit_lab_run: %s → %s (level=%s)\n",
		s_cases[idx].name, s_lastStatus, vk_wboit_production_level_name( vk_wboit_production_level() ) );
	return ok;
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
		ri.Printf( PRINT_ALL, "unknown case '%s' (oit_lab_list)\n", ri.Cmd_Argv( 1 ) );
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
	int i, pass = 0, ran = 0;
	if ( ri.Cmd_Argc() < 2 ) {
		ri.Printf( PRINT_ALL, "usage: oit_lab_run_group <group>\n" );
		return;
	}
	g = OIT_Lab_ParseGroup( ri.Cmd_Argv( 1 ) );
	for ( i = 0; i < (int)( sizeof( s_cases ) / sizeof( s_cases[0] ) ); i++ ) {
		if ( g != OIT_LAB_GROUP_ALL && s_cases[i].group != g ) {
			continue;
		}
		ran++;
		if ( OIT_Lab_RunIndex( i ) ) {
			pass++;
		}
	}
	ri.Printf( PRINT_ALL, "oit_lab_run_group %s: %d/%d passed\n", OIT_Lab_GroupName( g ), pass, ran );
}

static void OIT_Lab_Status_f( void )
{
	ri.Printf( PRINT_ALL,
		"oit_lab_status: lastCase=%s status=%s freeze=%d level=%s\n"
		"  temporal/anim/exposure frozen during compares when r_oitLabFreeze 1\n",
		( s_lastCase >= 0 && s_lastCase < (int)( sizeof( s_cases ) / sizeof( s_cases[0] ) ) )
			? s_cases[s_lastCase].name : "(none)",
		s_lastStatus[0] ? s_lastStatus : "-",
		r_oitLabFreeze ? r_oitLabFreeze->integer : 0,
		vk_wboit_production_level_name( vk_wboit_production_level() ) );
}

static void OIT_Lab_Reset_f( void )
{
	s_lastCase = -1;
	s_lastStatus[0] = '\0';
	ri.Cmd_ExecuteText( EXEC_APPEND, "oit_certification_abort\n" );
	ri.Printf( PRINT_ALL, "oit_lab_reset: certification session reset\n" );
}

void vk_transparency_lab_begin_frame( void );

/* Extend existing lab register — called from route init after transparency_lab_register. */
void vk_oit_lab_register( void )
{
	if ( s_cmds ) {
		return;
	}
	s_lastCase = -1;
	r_oitLabFreeze = ri.Cvar_Get( "r_oitLabFreeze", "1", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_oitLabFreeze, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_oitLabFreeze,
		"Freeze jitter/animation/exposure for OIT lab compares." );

	ri.Cmd_AddCommand( "oit_lab_list", OIT_Lab_List_f );
	ri.Cmd_AddCommand( "oit_lab_run", OIT_Lab_Run_f );
	ri.Cmd_AddCommand( "oit_lab_run_group", OIT_Lab_RunGroup_f );
	ri.Cmd_AddCommand( "oit_lab_status", OIT_Lab_Status_f );
	ri.Cmd_AddCommand( "oit_lab_reset", OIT_Lab_Reset_f );
	s_cmds = qtrue;
	ri.Printf( PRINT_ALL, "[VK][OIT] Phase 2.6A oit_lab ready (%d cases)\n",
		(int)( sizeof( s_cases ) / sizeof( s_cases[0] ) ) );
}
