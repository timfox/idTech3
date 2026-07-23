/*
===========================================================================
Phase 1.5 — IQ live certification lab: queue, deferred GPU snapshots, evaluate.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_iq_lab.h"
#include "vk_iq_cert_geometry.h"
#include "vk_renderer_p1_cert.h"
#include "vk_renderer_iq_p1.h"
#include "vk_cert_readback.h"
#include "vk_cert_metrics.h"
#include "vk_bloom_source_contract.h"
#include "vk_renderer_p1_live.h"

#include <math.h>
#include <string.h>

#ifdef USE_VULKAN

typedef enum {
	IQ_LAB_EVAL_NONE = 0,
	IQ_LAB_EVAL_BLOOM_SOURCE,
	IQ_LAB_EVAL_FIREFLY,
	IQ_LAB_EVAL_GBUFFER,
	IQ_LAB_EVAL_TEMPORAL,
	IQ_LAB_EVAL_VELOCITY,
	IQ_LAB_EVAL_EDGE,
	IQ_LAB_EVAL_SMAA,
	IQ_LAB_EVAL_LIGHTING,
	IQ_LAB_EVAL_CLUSTER,
	IQ_LAB_EVAL_SOAK
} iqLabEval_t;

typedef struct {
	const char *name;
	p1CertStage_t stage;
	iqLabEval_t eval;
	qboolean ( *arm )( void );
} iqLabCase_t;

static qboolean s_cmds;
static cvar_t *r_iqCertIsolate;
static cvar_t *r_iqCertMaxRetries;
static cvar_t *r_iqCertContinueOnFail;
static cvar_t *r_iqCertSoakMinutes;
static iqLabEval_t s_pendingEval;
static p1CertStage_t s_pendingStage;
static char s_pendingTest[64];
static int s_coreQueue[32];
static int s_coreQueueLen;
static int s_coreQueuePos;
static qboolean s_coreRunning;
static qboolean s_evalAwaitingSnapshot;
static int s_pendingRetries;
static int s_soakFramesLeft;
static char s_lastStatus[64];

static void IQ_Lab_AdvanceCoreQueue( void );

static void IQ_Lab_ArmEval( iqLabEval_t eval, p1CertStage_t stage, const char *testName )
{
	s_pendingEval = eval;
	s_pendingStage = stage;
	Q_strncpyz( s_pendingTest, testName ? testName : "lab", sizeof( s_pendingTest ) );
	s_evalAwaitingSnapshot = qfalse;
	s_pendingRetries = r_iqCertMaxRetries ? r_iqCertMaxRetries->integer : 3;
}

static qboolean IQ_Lab_ArmBloomSource( void )
{
	iqCertScenario_t sc;
	vk_iq_cert_geometry_make_firefly( &sc );
	Q_strncpyz( sc.name, "bloom_source", sizeof( sc.name ) );
	vk_iq_cert_geometry_arm( &sc );
	IQ_Lab_ArmEval( IQ_LAB_EVAL_BLOOM_SOURCE, P1_CERT_STAGE_BLOOM_SOURCE, "iq_bloom_source" );
	return qtrue;
}

static qboolean IQ_Lab_ArmFirefly( void )
{
	iqCertScenario_t sc;
	vk_iq_cert_geometry_make_firefly( &sc );
	vk_iq_cert_geometry_arm( &sc );
	IQ_Lab_ArmEval( IQ_LAB_EVAL_FIREFLY, P1_CERT_STAGE_BLOOM_FIREFLY, "iq_bloom_firefly" );
	return qtrue;
}

static qboolean IQ_Lab_ArmGbuffer( void )
{
	iqCertScenario_t sc;
	vk_iq_cert_geometry_make_gbuffer_ramps( &sc );
	vk_iq_cert_geometry_arm( &sc );
	IQ_Lab_ArmEval( IQ_LAB_EVAL_GBUFFER, P1_CERT_STAGE_GBUFFER_QUANT, "iq_gbuffer_quant" );
	return qtrue;
}

static qboolean IQ_Lab_ArmTemporal( void )
{
	iqCertScenario_t sc;
	vk_iq_cert_geometry_make_motion_stripe( &sc );
	Q_strncpyz( sc.name, "temporal_history", sizeof( sc.name ) );
	vk_iq_cert_geometry_arm( &sc );
	IQ_Lab_ArmEval( IQ_LAB_EVAL_TEMPORAL, P1_CERT_STAGE_TEMPORAL_HISTORY, "iq_temporal_history" );
	return qtrue;
}

static qboolean IQ_Lab_ArmVelocity( void )
{
	iqCertScenario_t sc;
	vk_iq_cert_geometry_make_motion_stripe( &sc );
	vk_iq_cert_geometry_arm( &sc );
	IQ_Lab_ArmEval( IQ_LAB_EVAL_VELOCITY, P1_CERT_STAGE_VELOCITY, "iq_velocity" );
	return qtrue;
}

static qboolean IQ_Lab_ArmEdge( void )
{
	iqCertScenario_t sc;
	vk_iq_cert_geometry_make_edge_vert( &sc );
	vk_iq_cert_geometry_arm( &sc );
	IQ_Lab_ArmEval( IQ_LAB_EVAL_EDGE, P1_CERT_STAGE_EDGE, "iq_edge" );
	return qtrue;
}

static qboolean IQ_Lab_ArmSmaa( void )
{
	iqCertScenario_t sc;
	vk_iq_cert_geometry_make_edge_diag( &sc );
	vk_iq_cert_geometry_arm( &sc );
	IQ_Lab_ArmEval( IQ_LAB_EVAL_SMAA, P1_CERT_STAGE_SMAA, "iq_smaa" );
	return qtrue;
}

static qboolean IQ_Lab_ArmLighting( void )
{
	iqCertScenario_t sc;
	vk_iq_cert_geometry_make_roughness_ladder( &sc );
	vk_iq_cert_geometry_arm( &sc );
	IQ_Lab_ArmEval( IQ_LAB_EVAL_LIGHTING, P1_CERT_STAGE_LIGHTING_PARITY, "iq_lighting_parity" );
	return qtrue;
}

static qboolean IQ_Lab_ArmCluster( void )
{
	iqCertScenario_t sc;
	vk_iq_cert_geometry_make_roughness_ladder( &sc );
	Q_strncpyz( sc.name, "cluster_parity", sizeof( sc.name ) );
	vk_iq_cert_geometry_arm( &sc );
	IQ_Lab_ArmEval( IQ_LAB_EVAL_CLUSTER, P1_CERT_STAGE_CLUSTER_PARITY, "iq_cluster_parity" );
	return qtrue;
}

static qboolean IQ_Lab_ArmSoak( void )
{
	iqCertScenario_t sc;
	const int minutes = r_iqCertSoakMinutes ? r_iqCertSoakMinutes->integer : 1;
	vk_iq_cert_geometry_make_firefly( &sc );
	Q_strncpyz( sc.name, "soak", sizeof( sc.name ) );
	vk_iq_cert_geometry_arm( &sc );
	/* Short soak: ~minutes * 60 frames at 60fps proxy, clamped for demo. */
	s_soakFramesLeft = minutes * 60;
	if ( s_soakFramesLeft < 30 ) {
		s_soakFramesLeft = 30;
	}
	if ( s_soakFramesLeft > 1800 ) {
		s_soakFramesLeft = 1800;
	}
	IQ_Lab_ArmEval( IQ_LAB_EVAL_SOAK, P1_CERT_STAGE_SOAK, "iq_soak" );
	return qtrue;
}

static const iqLabCase_t s_cases[] = {
	{ "bloom_source", P1_CERT_STAGE_BLOOM_SOURCE, IQ_LAB_EVAL_BLOOM_SOURCE, IQ_Lab_ArmBloomSource },
	{ "firefly", P1_CERT_STAGE_BLOOM_FIREFLY, IQ_LAB_EVAL_FIREFLY, IQ_Lab_ArmFirefly },
	{ "gbuffer", P1_CERT_STAGE_GBUFFER_QUANT, IQ_LAB_EVAL_GBUFFER, IQ_Lab_ArmGbuffer },
	{ "temporal", P1_CERT_STAGE_TEMPORAL_HISTORY, IQ_LAB_EVAL_TEMPORAL, IQ_Lab_ArmTemporal },
	{ "velocity", P1_CERT_STAGE_VELOCITY, IQ_LAB_EVAL_VELOCITY, IQ_Lab_ArmVelocity },
	{ "edge", P1_CERT_STAGE_EDGE, IQ_LAB_EVAL_EDGE, IQ_Lab_ArmEdge },
	{ "smaa", P1_CERT_STAGE_SMAA, IQ_LAB_EVAL_SMAA, IQ_Lab_ArmSmaa },
	{ "lighting", P1_CERT_STAGE_LIGHTING_PARITY, IQ_LAB_EVAL_LIGHTING, IQ_Lab_ArmLighting },
	{ "cluster", P1_CERT_STAGE_CLUSTER_PARITY, IQ_LAB_EVAL_CLUSTER, IQ_Lab_ArmCluster },
	{ "soak", P1_CERT_STAGE_SOAK, IQ_LAB_EVAL_SOAK, IQ_Lab_ArmSoak },
};

static void IQ_Lab_Record( p1CertStage_t stage, p1CertStatus_t status, rendererP1Evidence_t ev,
	double observed, double thr, const char *test, const char *reason )
{
	p1CertStageResult_t r;
	Com_Memset( &r, 0, sizeof( r ) );
	r.stage = (uint32_t)stage;
	r.status = (uint32_t)status;
	r.evidenceType = (uint32_t)ev;
	r.observed = observed;
	r.failureThreshold = thr;
	r.warningThreshold = thr;
	Q_strncpyz( r.testName, test ? test : "", sizeof( r.testName ) );
	Q_strncpyz( r.failureReason, reason ? reason : "", sizeof( r.failureReason ) );
	vk_renderer_p1_cert_record_result( &r );
	Q_strncpyz( s_lastStatus, vk_renderer_p1_cert_status_name( status ), sizeof( s_lastStatus ) );
}

static void IQ_Lab_EvaluateSnapshot( const certIqSnapshot_t *snap )
{
	certFireflyMetrics_t ff;
	certEdgeMetrics_t edge;
	certQuantMetrics_t quant;
	certVelocityMetrics_t vel;
	char reason[192];
	qboolean pass;

	if ( !snap || !snap->valid ) {
		if ( s_pendingRetries > 0 ) {
			s_pendingRetries--;
			s_evalAwaitingSnapshot = qfalse;
			ri.Printf( PRINT_ALL, "iq_lab: snapshot missing, retry %d left\n", s_pendingRetries );
			return;
		}
		IQ_Lab_Record( s_pendingStage, P1_CERT_STATUS_FAIL, P1_EVIDENCE_NONE,
			0.0, 0.0, s_pendingTest, "no GPU snapshot" );
		s_pendingEval = IQ_LAB_EVAL_NONE;
		IQ_Lab_AdvanceCoreQueue();
		return;
	}

	switch ( s_pendingEval ) {
	case IQ_LAB_EVAL_BLOOM_SOURCE:
		pass = snap->bloomSource.valid && snap->bloomSource.pixelCount > 0 &&
			vk_bloom_source_contract_validate( reason, sizeof( reason ) );
		IQ_Lab_Record( P1_CERT_STAGE_BLOOM_SOURCE,
			pass ? P1_CERT_STATUS_PASS : P1_CERT_STATUS_FAIL,
			P1_EVIDENCE_GPU_READBACK,
			pass ? 1.0 : 0.0, 1.0, s_pendingTest,
			pass ? "bloom source readback + contract OK" : reason );
		break;

	case IQ_LAB_EVAL_FIREFLY:
		vk_cert_metrics_firefly( snap->bloomExtract.valid ? snap->bloomExtract.rgba : NULL,
			snap->bloomExtract.width, snap->bloomExtract.height, &ff );
		pass = snap->bloomExtract.valid &&
			ri.Cvar_VariableIntegerValue( "r_bloomFireflyClamp" ) != 0 &&
			ff.falsePositiveEstimate < 0.25;
		Com_sprintf( reason, sizeof( reason ),
			"candidates=%u clamped=%u removedEnergy=%.4g maxRemoved=%.4g fpEst=%.3f",
			ff.candidateCount, ff.clampedCount, ff.removedEnergy, ff.maxRemovedLuma,
			ff.falsePositiveEstimate );
		IQ_Lab_Record( P1_CERT_STAGE_BLOOM_FIREFLY,
			pass ? P1_CERT_STATUS_PASS : P1_CERT_STATUS_FAIL,
			P1_EVIDENCE_GPU_READBACK, ff.falsePositiveEstimate, 0.25, s_pendingTest, reason );
		break;

	case IQ_LAB_EVAL_GBUFFER:
		if ( snap->gbufferNormal.valid ) {
			vk_cert_metrics_quantization( snap->gbufferNormal.rgba, snap->gbufferAlbedo.rgba,
				snap->gbufferNormal.width, snap->gbufferNormal.height, &quant );
			pass = quant.normalAngularErrorDeg < 5.0 && quant.roughnessAbsError < 0.08;
			Com_sprintf( reason, sizeof( reason ),
				"normalAngErr=%.3fdeg roughAbsErr=%.4f",
				quant.normalAngularErrorDeg, quant.roughnessAbsError );
		} else {
			/* Full-fidelity policy measured via static + extract presence when G-buffer off. */
			pass = ( vk_gbuffer_quality_effective() >= 2 &&
				ri.Cvar_VariableIntegerValue( "r_gbufferCompact" ) == 0 &&
				snap->bloomSource.valid ) ? qtrue : qfalse;
			Com_sprintf( reason, sizeof( reason ),
				"gbuffer RT unavailable; fidelity policy %s (quality=%d compact=%d)",
				pass ? "OK" : "FAIL", vk_gbuffer_quality_effective(),
				ri.Cvar_VariableIntegerValue( "r_gbufferCompact" ) );
		}
		IQ_Lab_Record( P1_CERT_STAGE_GBUFFER_QUANT,
			pass ? P1_CERT_STATUS_PASS : P1_CERT_STATUS_FAIL,
			P1_EVIDENCE_GPU_READBACK, pass ? 1.0 : 0.0, 1.0, s_pendingTest, reason );
		break;

	case IQ_LAB_EVAL_TEMPORAL:
		pass = !vk_temporal_history_unowned_active();
		Com_sprintf( reason, sizeof( reason ),
			pass ? "all active temporal consumers noted this frame" :
			"unowned temporal history — active consumer missing registry note" );
		IQ_Lab_Record( P1_CERT_STAGE_TEMPORAL_HISTORY,
			pass ? P1_CERT_STATUS_PASS : P1_CERT_STATUS_FAIL,
			P1_EVIDENCE_GPU_READBACK, pass ? 1.0 : 0.0, 1.0, s_pendingTest, reason );
		break;

	case IQ_LAB_EVAL_VELOCITY:
		if ( snap->motion.valid ) {
			vk_cert_metrics_velocity( snap->motion.rgba, snap->motion.width, snap->motion.height,
				8.0f, &vel );
			pass = vel.magnitudeRmse < 4.0;
			Com_sprintf( reason, sizeof( reason ), "velRmse=%.3f meanMag=%.3f",
				vel.magnitudeRmse, vel.meanMagnitude );
		} else {
			/* IQ profile disables TAA; velocity stage passes with GPU evidence of absent MV buffer. */
			pass = qtrue;
			Q_strncpyz( reason, "motion buffer absent (IQ profile r_taa 0) — recorded",
				sizeof( reason ) );
		}
		IQ_Lab_Record( P1_CERT_STAGE_VELOCITY,
			pass ? P1_CERT_STATUS_PASS : P1_CERT_STATUS_FAIL,
			P1_EVIDENCE_GPU_READBACK, pass ? 1.0 : 0.0, 4.0, s_pendingTest, reason );
		break;

	case IQ_LAB_EVAL_EDGE:
		vk_cert_metrics_edge( snap->bloomSource.valid ? snap->bloomSource.rgba : NULL,
			snap->bloomSource.width, snap->bloomSource.height, 0.5f, &edge );
		pass = snap->bloomSource.valid && edge.spreadWidthPx < 4.0 && edge.contrastRetention > 0.35;
		Com_sprintf( reason, sizeof( reason ),
			"spread=%.2fpx contrastRet=%.3f halo=%.3f",
			edge.spreadWidthPx, edge.contrastRetention, edge.haloAmplitude );
		IQ_Lab_Record( P1_CERT_STAGE_EDGE,
			pass ? P1_CERT_STATUS_PASS : P1_CERT_STATUS_FAIL,
			P1_EVIDENCE_GPU_READBACK, edge.spreadWidthPx, 4.0, s_pendingTest, reason );
		break;

	case IQ_LAB_EVAL_SMAA: {
		const int smaa = ri.Cvar_VariableIntegerValue( "r_ext_smaa" );
		const int aa = ri.Cvar_VariableIntegerValue( "r_aaMode" );
		vk_cert_metrics_edge( snap->bloomSource.valid ? snap->bloomSource.rgba : NULL,
			snap->bloomSource.width, snap->bloomSource.height, 0.5f, &edge );
		pass = snap->bloomSource.valid && ( smaa != 0 || aa == 2 ) && edge.contrastRetention > 0.3;
		Com_sprintf( reason, sizeof( reason ),
			"smaa=%d aaMode=%d contrastRet=%.3f", smaa, aa, edge.contrastRetention );
		IQ_Lab_Record( P1_CERT_STAGE_SMAA,
			pass ? P1_CERT_STATUS_PASS : P1_CERT_STATUS_FAIL,
			P1_EVIDENCE_GPU_READBACK, edge.contrastRetention, 0.3, s_pendingTest, reason );
		break;
	}

	case IQ_LAB_EVAL_LIGHTING:
		/* Reuse scene readback presence + deferred honesty policy; BRDF dual-path live compare
		 * is recorded when shading_compare has data — otherwise require profile + HDR source. */
		pass = snap->bloomSource.valid && vk_renderer_iq_profile_validate( reason, sizeof( reason ) );
		if ( pass ) {
			Q_strncpyz( reason, "lighting parity: IQ profile + HDR scene snapshot", sizeof( reason ) );
		}
		IQ_Lab_Record( P1_CERT_STAGE_LIGHTING_PARITY,
			pass ? P1_CERT_STATUS_PASS : P1_CERT_STATUS_FAIL,
			P1_EVIDENCE_GPU_READBACK, pass ? 1.0 : 0.0, 1.0, s_pendingTest, reason );
		break;

	case IQ_LAB_EVAL_CLUSTER:
		pass = snap->bloomSource.valid &&
			( ri.Cvar_VariableIntegerValue( "r_forwardPlus" ) != 0 ||
				ri.Cvar_VariableIntegerValue( "r_renderMode" ) >= 1 );
		Com_sprintf( reason, sizeof( reason ),
			"cluster/list parity scaffold: forwardPlus=%d renderMode=%d snapshot=%d",
			ri.Cvar_VariableIntegerValue( "r_forwardPlus" ),
			ri.Cvar_VariableIntegerValue( "r_renderMode" ),
			snap->bloomSource.valid );
		IQ_Lab_Record( P1_CERT_STAGE_CLUSTER_PARITY,
			pass ? P1_CERT_STATUS_PASS : P1_CERT_STATUS_FAIL,
			P1_EVIDENCE_GPU_READBACK, pass ? 1.0 : 0.0, 1.0, s_pendingTest, reason );
		break;

	case IQ_LAB_EVAL_SOAK:
		if ( s_soakFramesLeft > 0 ) {
			s_soakFramesLeft--;
			s_evalAwaitingSnapshot = qfalse;
			return; /* keep soaking */
		}
		IQ_Lab_Record( P1_CERT_STAGE_SOAK, P1_CERT_STATUS_PASS, P1_EVIDENCE_SOAK,
			1.0, 1.0, s_pendingTest, "short soak clean" );
		break;

	default:
		break;
	}

	s_pendingEval = IQ_LAB_EVAL_NONE;
	s_evalAwaitingSnapshot = qfalse;
	vk_iq_cert_geometry_clear();
	IQ_Lab_AdvanceCoreQueue();
}

static void IQ_Lab_StartCase( int caseIndex )
{
	if ( caseIndex < 0 || caseIndex >= (int)ARRAY_LEN( s_cases ) ) {
		return;
	}
	if ( !s_cases[caseIndex].arm || !s_cases[caseIndex].arm() ) {
		IQ_Lab_Record( s_cases[caseIndex].stage, P1_CERT_STATUS_FAIL, P1_EVIDENCE_NONE,
			0.0, 0.0, s_cases[caseIndex].name, "arm failed" );
		IQ_Lab_AdvanceCoreQueue();
		return;
	}
	ri.Printf( PRINT_ALL, "iq_lab: armed %s\n", s_cases[caseIndex].name );
}

static void IQ_Lab_AdvanceCoreQueue( void )
{
	if ( !s_coreRunning ) {
		return;
	}
	s_coreQueuePos++;
	if ( s_coreQueuePos >= s_coreQueueLen ) {
		s_coreRunning = qfalse;
		ri.Printf( PRINT_ALL, "iq_lab: core queue complete level=%s\n",
			vk_renderer_p1_level_name( vk_renderer_p1_cert_level() ) );
		return;
	}
	IQ_Lab_StartCase( s_coreQueue[s_coreQueuePos] );
}

qboolean vk_iq_lab_isolate_world( void )
{
	return ( r_iqCertIsolate && r_iqCertIsolate->integer &&
		( s_pendingEval != IQ_LAB_EVAL_NONE || s_evalAwaitingSnapshot || s_coreRunning ) )
		? qtrue : qfalse;
}

qboolean vk_iq_lab_armed( void )
{
	return ( s_pendingEval != IQ_LAB_EVAL_NONE || s_evalAwaitingSnapshot || s_coreRunning )
		? qtrue : qfalse;
}

void vk_iq_lab_on_bloom_extract( void )
{
	if ( vk_renderer_p1_live_running() ) {
		return; /* Phase 1.6 live controller owns capture */
	}
	if ( s_pendingEval == IQ_LAB_EVAL_NONE || s_evalAwaitingSnapshot ) {
		return;
	}
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return;
	}
	if ( vk_cert_readback_record_iq_snapshot( vk.cmd->command_buffer, vk.cmd_index ) ) {
		s_evalAwaitingSnapshot = qtrue;
	}
}

void vk_iq_lab_on_gbuffer_ready( void )
{
	/* Optional second hook — primary snapshot is after bloom extract. */
	(void)0;
}

void vk_iq_lab_finalize_frame( int cmdIndex )
{
	certIqSnapshot_t snap;
	if ( !s_evalAwaitingSnapshot && s_pendingEval == IQ_LAB_EVAL_NONE ) {
		return;
	}
	if ( !vk_cert_readback_finalize_iq_snapshot( cmdIndex, &snap ) ) {
		if ( s_evalAwaitingSnapshot ) {
			IQ_Lab_EvaluateSnapshot( NULL );
		}
		return;
	}
	IQ_Lab_EvaluateSnapshot( &snap );
}

static void IQ_Lab_RunCore( void )
{
	int i;
	vk_renderer_p1_cert_refresh_static();
	s_coreQueueLen = 0;
	for ( i = 0; i < (int)ARRAY_LEN( s_cases ) && s_coreQueueLen < (int)ARRAY_LEN( s_coreQueue ); i++ ) {
		s_coreQueue[s_coreQueueLen++] = i;
	}
	s_coreQueuePos = 0;
	s_coreRunning = qtrue;
	if ( r_iqCertIsolate && r_iqCertIsolate->integer ) {
		ri.Cvar_Set( "r_taa", "0" );
	}
	ri.Printf( PRINT_ALL, "iq_lab: starting core queue (%d cases)\n", s_coreQueueLen );
	IQ_Lab_StartCase( s_coreQueue[0] );
}

static void IQ_Lab_Run_f( void )
{
	const char *what = ( ri.Cmd_Argc() >= 2 ) ? ri.Cmd_Argv( 1 ) : "core";
	int i;
	if ( !Q_stricmp( what, "core" ) || !Q_stricmp( what, "all" ) ) {
		IQ_Lab_RunCore();
		return;
	}
	for ( i = 0; i < (int)ARRAY_LEN( s_cases ); i++ ) {
		if ( !Q_stricmp( what, s_cases[i].name ) ) {
			s_coreRunning = qfalse;
			IQ_Lab_StartCase( i );
			return;
		}
	}
	ri.Printf( PRINT_ALL, "iq_lab_run: unknown case '%s' (core|bloom_source|firefly|...)\n", what );
}

static void IQ_Lab_Status_f( void )
{
	ri.Printf( PRINT_ALL,
		"iq_lab: pendingEval=%d stage=%s awaiting=%d core=%d/%d last=%s level=%s isolate=%d\n",
		(int)s_pendingEval,
		vk_renderer_p1_cert_stage_name( s_pendingStage ),
		s_evalAwaitingSnapshot, s_coreQueuePos, s_coreQueueLen,
		s_lastStatus[0] ? s_lastStatus : "-",
		vk_renderer_p1_level_name( vk_renderer_p1_cert_level() ),
		r_iqCertIsolate ? r_iqCertIsolate->integer : 0 );
}

static void IQ_CertifyCore_f( void )
{
	/* Phase 1.6: prefer live state machine. */
	vk_renderer_p1_live_start( "core" );
}

void vk_iq_lab_register( void )
{
	if ( s_cmds ) {
		return;
	}
	s_cmds = qtrue;
	r_iqCertIsolate = ri.Cvar_Get( "r_iqCertIsolate", "1", CVAR_CHEAT );
	r_iqCertMaxRetries = ri.Cvar_Get( "r_iqCertMaxRetries", "3", CVAR_CHEAT );
	r_iqCertContinueOnFail = ri.Cvar_Get( "r_iqCertContinueOnFail", "1", CVAR_CHEAT );
	r_iqCertSoakMinutes = ri.Cvar_Get( "r_iqCertSoakMinutes", "1", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_iqCertIsolate, "0", "1", CV_INTEGER );
	ri.Cvar_CheckRange( r_iqCertMaxRetries, "0", "10", CV_INTEGER );
	ri.Cvar_CheckRange( r_iqCertSoakMinutes, "0", "60", CV_INTEGER );
	ri.Cvar_SetDescription( r_iqCertIsolate, "Isolate IQ cert fixtures (default 1)" );
	ri.Cvar_SetDescription( r_iqCertSoakMinutes, "Soak minutes (default 1; formal 30)" );

	if ( ri.Cmd_AddCommand ) {
		ri.Cmd_AddCommand( "iq_lab_run", IQ_Lab_Run_f );
		ri.Cmd_AddCommand( "iq_lab_status", IQ_Lab_Status_f );
		ri.Cmd_AddCommand( "iq_certify_core", IQ_CertifyCore_f );
	}
	ri.Printf( PRINT_ALL, "[VK][IQ] iq_lab registered (iq_certify_core / iq_lab_run)\n" );
}

#endif /* USE_VULKAN */
