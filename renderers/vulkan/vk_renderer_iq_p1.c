/*
===========================================================================
Renderer IQ P1 hub — profile, history registry, ghost isolation, certification.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_renderer_iq_p1.h"
#include "vk_renderer_p1_cert.h"
#include "vk_renderer_p1_live.h"
#include "vk_iq_lab.h"
#include "vk_iq_cert_geometry.h"
#include "vk_bloom_source_contract.h"
#include "vk_color_contract.h"
#include "vk_scene_hdr_ownership.h"
#include "vk_aa_policy.h"
#include "vk_render_path.h"
#include "vk_util.h"
#include <stdlib.h>

#ifdef USE_VULKAN

typedef struct {
	qboolean valid;
	qboolean notedThisFrame;
	char resetReason[48];
	uint32_t noteCount;
	uint32_t resetCount;
} historySlot_t;

static historySlot_t s_hist[HISTORY_OWNER_COUNT];
static qboolean s_cmds;
static cvar_t *r_ghostIsolation;
static cvar_t *r_gbufferQuality;
static cvar_t *r_bloomFireflyClamp;
static cvar_t *r_bloomFireflyRatio;
static cvar_t *r_bloomFireflyAbsolute;
static cvar_t *r_bloomFireflyNeighborhood;
static cvar_t *r_bloomFireflyDebug;
static cvar_t *r_bloomTemporalOrder;
static cvar_t *r_bloomGhostingDebug;
static cvar_t *r_bloomDebug;
static cvar_t *r_ssrTemporal;
static cvar_t *r_allowExperimentalTemporalSSR;
static uint32_t s_p1GatePass[P1_GATE_COUNT];
static uint32_t s_p1GateFail[P1_GATE_COUNT];
static rendererP1Evidence_t s_gateEvidence[P1_GATE_COUNT];

static const char *HistoryOwnerName( rendererHistoryOwner_t o )
{
	switch ( o ) {
	case HISTORY_TAA: return "taa";
	case HISTORY_WEAPON: return "weapon";
	case HISTORY_SSR: return "ssr";
	case HISTORY_AO: return "ao";
	case HISTORY_VOLUMETRIC: return "volumetric";
	case HISTORY_EXPOSURE: return "exposure";
	case HISTORY_BLOOM: return "bloom";
	case HISTORY_SHADOW: return "shadow";
	case HISTORY_TRANSPARENCY: return "transparency";
	case HISTORY_OTHER: return "other";
	default: return "unknown";
	}
}

static const char *P1GateName( rendererP1Gate_t g )
{
	switch ( g ) {
	case P1_GATE_BLOOM_SOURCE: return "BLOOM_SOURCE_CERTIFIED";
	case P1_GATE_BLOOM_FIREFLY: return "BLOOM_FIREFLY_CONTROL_CERTIFIED";
	case P1_GATE_NO_UNOWNED_HISTORY: return "NO_UNOWNED_TEMPORAL_HISTORY";
	case P1_GATE_VELOCITY: return "VELOCITY_CERTIFIED";
	case P1_GATE_SPECULAR_STABILITY: return "SPECULAR_STABILITY_CERTIFIED";
	case P1_GATE_GBUFFER_FULL_FIDELITY: return "GBUFFER_FULL_FIDELITY_CERTIFIED";
	case P1_GATE_DEFERRED_FORWARD_PARITY: return "DEFERRED_FORWARD_PARITY_CERTIFIED";
	case P1_GATE_LIGHTING_OWNERSHIP: return "LIGHTING_OWNERSHIP_CERTIFIED";
	case P1_GATE_CLUSTER_PARITY: return "CLUSTER_PARITY_CERTIFIED";
	case P1_GATE_EDGE_REFERENCE: return "EDGE_REFERENCE_CERTIFIED";
	case P1_GATE_SMAA: return "SMAA_CERTIFIED";
	case P1_GATE_MSAA_POLICY: return "MSAA_POLICY_CERTIFIED";
	case P1_GATE_TEXTURE_LOD: return "TEXTURE_LOD_CERTIFIED";
	default: return "UNKNOWN";
	}
}

void vk_renderer_iq_p1_begin_frame( void )
{
	int i;
	for ( i = 0; i < HISTORY_OWNER_COUNT; i++ ) {
		s_hist[i].notedThisFrame = qfalse;
	}
	/* Disabled consumers still must register ownership (inactive) each frame. */
	if ( !r_taa || !r_taa->integer ) {
		vk_temporal_history_note( HISTORY_TAA, qfalse, "r_taa 0" );
	}
	/* Production/current-frame SSR never registers a temporal history owner. */
	vk_temporal_history_note( HISTORY_SSR, qfalse,
		( r_ssrTemporal && r_ssrTemporal->integer &&
		  r_allowExperimentalTemporalSSR && r_allowExperimentalTemporalSSR->integer )
			? "experimental temporal SSR uncertified"
			: "temporal SSR quarantined" );
	if ( !ri.Cvar_VariableIntegerValue( "r_ssao" ) ||
		!ri.Cvar_VariableIntegerValue( "r_temporalAO" ) ) {
		vk_temporal_history_note( HISTORY_AO, qfalse, "ao temporal off" );
	}
	if ( !ri.Cvar_VariableIntegerValue( "r_volumetricFog" ) ||
		!ri.Cvar_VariableIntegerValue( "r_temporalFog" ) ) {
		vk_temporal_history_note( HISTORY_VOLUMETRIC, qfalse, "fog temporal off" );
	}
	if ( !ri.Cvar_VariableIntegerValue( "r_temporalWeapon" ) &&
		!ri.Cvar_VariableIntegerValue( "r_temporalWeaponAfterTaa" ) ) {
		vk_temporal_history_note( HISTORY_WEAPON, qfalse, "weapon temporal off" );
	}
	vk_temporal_history_note( HISTORY_BLOOM, qfalse, "bloom extract non-temporal" );
	vk_temporal_history_note( HISTORY_EXPOSURE, qfalse, "exposure default" );
	vk_renderer_p1_cert_begin_frame();
	vk_renderer_p1_live_begin_frame();
}

void vk_temporal_history_note( rendererHistoryOwner_t owner, qboolean valid,
	const char *resetReason )
{
	historySlot_t *s;

	if ( owner < 0 || owner >= HISTORY_OWNER_COUNT ) {
		return;
	}
	s = &s_hist[owner];
	s->noteCount++;
	s->notedThisFrame = qtrue;
	if ( !valid && s->valid ) {
		s->resetCount++;
	}
	s->valid = valid;
	if ( resetReason && resetReason[0] ) {
		Q_strncpyz( s->resetReason, resetReason, sizeof( s->resetReason ) );
	} else if ( valid ) {
		s->resetReason[0] = '\0';
	}
}

qboolean vk_temporal_history_noted_this_frame( rendererHistoryOwner_t owner )
{
	if ( owner < 0 || owner >= HISTORY_OWNER_COUNT ) {
		return qfalse;
	}
	return s_hist[owner].notedThisFrame;
}

qboolean vk_temporal_history_unowned_active( void )
{
	/* Active temporal consumers must note ownership each frame when enabled. */
	if ( r_taa && r_taa->integer && !s_hist[HISTORY_TAA].notedThisFrame ) {
		return qtrue;
	}
	/* Quarantined SSR has no production history allocation to own. */
	if ( ri.Cvar_VariableIntegerValue( "r_ssao" ) &&
		ri.Cvar_VariableIntegerValue( "r_temporalAO" ) &&
		!s_hist[HISTORY_AO].notedThisFrame ) {
		return qtrue;
	}
	if ( ri.Cvar_VariableIntegerValue( "r_volumetricFog" ) &&
		ri.Cvar_VariableIntegerValue( "r_temporalFog" ) &&
		!s_hist[HISTORY_VOLUMETRIC].notedThisFrame ) {
		return qtrue;
	}
	if ( ri.Cvar_VariableIntegerValue( "r_temporalWeapon" ) &&
		!s_hist[HISTORY_WEAPON].notedThisFrame ) {
		return qtrue;
	}
	return qfalse;
}

int vk_ghost_isolation_mode( void )
{
	return r_ghostIsolation ? r_ghostIsolation->integer : 0;
}

int vk_gbuffer_quality_effective( void )
{
	const int q = r_gbufferQuality ? r_gbufferQuality->integer : 2;
	if ( q <= 0 ) {
		return 0;
	}
	if ( q == 1 ) {
		return 1;
	}
	return 2;
}

void vk_renderer_iq_profile_apply( void )
{
	/* modern_raster_iq_reference — native, no TAA, full G-buffer, WBOIT, bloom. */
	ri.Cvar_Set( "r_oit", "1" );
	ri.Cvar_Set( "r_oitFogMode", "1" );
	ri.Cvar_Set( "r_oitAllowExperimentalMboit", "0" );
	ri.Cvar_Set( "r_taa", "0" );
	ri.Cvar_Set( "r_motionBlur", "0" );
	ri.Cvar_Set( "r_dof", "0" );
	ri.Cvar_Set( "r_bloom", "1" );
	ri.Cvar_Set( "r_bloomTemporalOrder", "1" );
	ri.Cvar_Set( "r_bloomFireflyClamp", "1" );
	ri.Cvar_Set( "r_gbufferCompact", "0" );
	ri.Cvar_Set( "r_gbufferQuality", "2" );
	ri.Cvar_Set( "r_aaMode", "2" );
	ri.Cvar_Set( "r_ext_smaa", "1" );
	ri.Cvar_Set( "r_ext_fxaa", "0" );
	ri.Cvar_Set( "r_renderScale", "1.0" );
	ri.Cvar_Set( "r_sharpen", "0" );
	ri.Cvar_Set( "r_postAaAfterBloom", "0" );
	ri.Cvar_Set( "r_weaponBloomMode", "1" );
	ri.Cvar_Set( "r_temporalWeaponAfterTaa", "1" );
	ri.Cvar_Set( "r_fbo", "1" );
	ri.Cvar_Set( "r_ssrTemporal", "0" );
	ri.Cvar_Set( "r_temporalSSR", "0" );
	ri.Cvar_Set( "r_allowExperimentalTemporalSSR", "0" );
	ri.Printf( PRINT_ALL,
		"[VK][IQ] applied modern_raster_iq_reference (latched cvars need vid_restart)\n" );
}

static void SsrTemporalStatus_f( void )
{
	const qboolean requested = ( r_ssrTemporal && r_ssrTemporal->integer ) ? qtrue : qfalse;
	const qboolean allowed = ( r_allowExperimentalTemporalSSR &&
		r_allowExperimentalTemporalSSR->integer ) ? qtrue : qfalse;
	ri.Printf( PRINT_ALL,
		"Temporal SSR: requested=%d permission=%d effective=%d certification=%s\n"
		"  stable SSR path: %s\n"
		"  history allocation: none\n"
		"  history sampling: none\n"
		"  production history owner: no\n"
		"  SceneHDR temporal modification: no\n",
		requested, allowed, requested && allowed,
		requested && allowed ? "EXPERIMENTAL_UNCERTIFIED" : "QUARANTINED",
		ri.Cvar_VariableIntegerValue( "r_ssr" ) ? "current-frame-only" : "disabled" );
	if ( requested && allowed ) {
		ri.Printf( PRINT_WARNING,
			"Temporal SSR is experimental and not IQ-certified.\n" );
	}
}

static void SsrTemporalValidate_f( void )
{
	const qboolean requested = ( r_ssrTemporal && r_ssrTemporal->integer ) ? qtrue : qfalse;
	const qboolean allowed = ( r_allowExperimentalTemporalSSR &&
		r_allowExperimentalTemporalSSR->integer ) ? qtrue : qfalse;
	if ( requested && !allowed ) {
		ri.Printf( PRINT_ERROR,
			"ssr_temporal_validate: FAIL request bypassed quarantine policy\n" );
		return;
	}
	ri.Printf( PRINT_ALL,
		"ssr_temporal_validate: PASS (%s; zero production history)\n",
		requested && allowed ? "experimental/uncertified" : "stable/quarantined" );
}

qboolean vk_renderer_iq_profile_validate( char *errBuf, int errBufSize )
{
	if ( r_taa && r_taa->integer ) {
		if ( errBuf && errBufSize > 0 ) {
			Com_sprintf( errBuf, errBufSize, "r_taa must be 0 for IQ reference" );
		}
		return qfalse;
	}
	if ( r_oit && r_oit->integer != 1 ) {
		if ( errBuf && errBufSize > 0 ) {
			Com_sprintf( errBuf, errBufSize, "r_oit must be 1 (WBOIT production)" );
		}
		return qfalse;
	}
	if ( r_gbufferCompact && r_gbufferCompact->integer ) {
		if ( errBuf && errBufSize > 0 ) {
			Com_sprintf( errBuf, errBufSize, "r_gbufferCompact must be 0 for IQ reference" );
		}
		return qfalse;
	}
	if ( r_bloom && !r_bloom->integer ) {
		if ( errBuf && errBufSize > 0 ) {
			Com_sprintf( errBuf, errBufSize, "r_bloom must be 1 for IQ reference" );
		}
		return qfalse;
	}
	if ( r_bloomFireflyClamp && !r_bloomFireflyClamp->integer ) {
		if ( errBuf && errBufSize > 0 ) {
			Com_sprintf( errBuf, errBufSize, "r_bloomFireflyClamp must be 1" );
		}
		return qfalse;
	}
	return qtrue;
}

static void IQ_ProfileStatus_f( void )
{
	char err[128];
	const qboolean ok = vk_renderer_iq_profile_validate( err, sizeof( err ) );

	ri.Printf( PRINT_ALL,
		"=== modern_raster_iq_reference ===\n"
		"  r_oit=%d r_oitFogMode=%d r_taa=%d r_bloom=%d fireflyClamp=%d\n"
		"  r_gbufferCompact=%d r_gbufferQuality=%d r_aaMode=%d r_sharpen=%s\n"
		"  r_renderScale=%s temporalReconstruction=%s\n"
		"  validate=%s\n"
		"  baseline: native res, no TAA, full G-buffer, WBOIT, bloom on\n",
		r_oit ? r_oit->integer : -1,
		ri.Cvar_VariableIntegerValue( "r_oitFogMode" ),
		r_taa ? r_taa->integer : -1,
		r_bloom ? r_bloom->integer : -1,
		r_bloomFireflyClamp ? r_bloomFireflyClamp->integer : -1,
		r_gbufferCompact ? r_gbufferCompact->integer : -1,
		r_gbufferQuality ? r_gbufferQuality->integer : -1,
		ri.Cvar_VariableIntegerValue( "r_aaMode" ),
		ri.Cvar_VariableString( "r_sharpen" ),
		ri.Cvar_VariableString( "r_renderScale" ),
		( r_taa && r_taa->integer ) ? "ON" : "OFF",
		ok ? "OK" : err );
}

static void IQ_ProfileValidate_f( void )
{
	char err[128];
	if ( vk_renderer_iq_profile_validate( err, sizeof( err ) ) ) {
		ri.Printf( PRINT_ALL, "renderer_iq_profile_validate: OK\n" );
	} else {
		ri.Printf( PRINT_ALL, "renderer_iq_profile_validate: FAIL (%s)\n", err );
	}
}

static void IQ_ProfileApply_f( void )
{
	vk_renderer_iq_profile_apply();
}

void vk_temporal_history_status_f( void )
{
	int i;

	ri.Printf( PRINT_ALL, "=== Temporal history registry (IQ P1-F) ===\n" );
	for ( i = 0; i < HISTORY_OWNER_COUNT; i++ ) {
		const historySlot_t *s = &s_hist[i];
		ri.Printf( PRINT_ALL,
			"  %-14s valid=%d notes=%u resets=%u reason=%s\n",
			HistoryOwnerName( (rendererHistoryOwner_t)i ),
			s->valid ? 1 : 0, s->noteCount, s->resetCount,
			s->resetReason[0] ? s->resetReason : "-" );
	}
	ri.Printf( PRINT_ALL,
		"  isolation=r_ghostIsolation %d (0=all 1=noTAA … 7=none)\n",
		vk_ghost_isolation_mode() );
}

static void GhostIsolationStatus_f( void )
{
	ri.Printf( PRINT_ALL,
		"=== Ghost isolation (IQ P1-G) ===\n"
		"  mode=%d\n"
		"  0 all configured | 1 no TAA | 2 no SSR | 3 no AO | 4 no volumetric\n"
		"  5 fixed exposure | 6 no weapon history | 7 no temporal systems\n"
		"  Use with bright moving specular / pan / weapon / glass / particles.\n",
		vk_ghost_isolation_mode() );
}

static void BloomPyramidStatus_f( void )
{
	int i;

	ri.Printf( PRINT_ALL,
		"=== Bloom pyramid (IQ P1-D) ===\n"
		"  passes=%d format=%s fireflyClamp=%d ratio=%g abs=%g neigh=%d debug=%d\n"
		"  temporalOrder=%d (1=after weapon when mode1) ghostDebug=%d bloomDebug=%d\n",
		VK_NUM_BLOOM_PASSES,
		vk_format_string( vk.bloom_format ),
		r_bloomFireflyClamp ? r_bloomFireflyClamp->integer : 0,
		r_bloomFireflyRatio ? r_bloomFireflyRatio->value : 0.0f,
		r_bloomFireflyAbsolute ? r_bloomFireflyAbsolute->value : 0.0f,
		r_bloomFireflyNeighborhood ? r_bloomFireflyNeighborhood->integer : 0,
		r_bloomFireflyDebug ? r_bloomFireflyDebug->integer : 0,
		r_bloomTemporalOrder ? r_bloomTemporalOrder->integer : 0,
		r_bloomGhostingDebug ? r_bloomGhostingDebug->integer : 0,
		r_bloomDebug ? r_bloomDebug->integer : 0 );
	for ( i = 0; i < VK_NUM_BLOOM_PASSES * 2 + 1 && i < (int)ARRAY_LEN( vk.bloom_mip_extent ); i++ ) {
		ri.Printf( PRINT_ALL, "  mip[%d] extent=%ux%u\n",
			i, vk.bloom_mip_extent[i].width, vk.bloom_mip_extent[i].height );
	}
	ri.Printf( PRINT_ALL,
		"  kernels: extract(soft knee+firefly) → blit downsample → 5-tap Gaussian H/V → additive blend\n"
		"  Commands: bloom_filter_status (alias), bloom_ghosting_status\n" );
}

static void BloomGhostingStatus_f( void )
{
	ri.Printf( PRINT_ALL,
		"=== Bloom ghosting attribution (IQ P1-E) ===\n"
		"  Bloom extract has no temporal history by default.\n"
		"  If trails remain with r_taa 0 + fireflyClamp 1, check:\n"
		"    HISTORY_SSR / AO / VOLUMETRIC / EXPOSURE / specular instability / stale SceneHDR\n"
		"  r_bloomGhostingDebug=%d r_ghostIsolation=%d\n",
		r_bloomGhostingDebug ? r_bloomGhostingDebug->integer : 0,
		vk_ghost_isolation_mode() );
}

static void GbufferQualityStatus_f( void )
{
	const int q = vk_gbuffer_quality_effective();

	ri.Printf( PRINT_ALL,
		"=== G-buffer quality (IQ P1-L) ===\n"
		"  r_gbufferQuality=%d effective=%d (%s)\n"
		"  r_gbufferCompact=%d (must be 0 for IQ reference / parity evidence)\n"
		"  0=compact/perf  1=balanced  2=full-fidelity IQ reference\n",
		r_gbufferQuality ? r_gbufferQuality->integer : -1, q,
		q == 2 ? "full-fidelity" : ( q == 1 ? "balanced" : "compact" ),
		r_gbufferCompact ? r_gbufferCompact->integer : -1 );
}

static void MsaaPolicyStatus_f( void )
{
	const int msaa = ri.Cvar_VariableIntegerValue( "r_ext_multisample" );
	const int oit = r_oit ? r_oit->integer : 0;

	ri.Printf( PRINT_ALL,
		"=== MSAA × Deferred × OIT policy (IQ P1-U) ===\n"
		"  r_ext_multisample=%d r_oit=%d\n"
		"  Forward+ opaque: native MSAA allowed\n"
		"  Deferred: prefer resolve to single-sample G-buffer consumers\n"
		"  WBOIT production: single-sample required (MSAA×OIT not certified)\n"
		"  Postprocess: resolved single-sample HDR\n",
		msaa, oit );
	if ( msaa > 0 && oit >= 1 ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
			"[VK][MSAA] unsupported combo: MSAA + OIT — prefer r_ext_multisample 0\n"
			S_COLOR_WHITE );
	}
}

static void EvaluateP1Gates( void )
{
	char err[128];
	int i;

	for ( i = 0; i < P1_GATE_COUNT; i++ ) {
		qboolean pass = qfalse;
		rendererP1Evidence_t ev = P1_EVIDENCE_STATIC;

		switch ( (rendererP1Gate_t)i ) {
		case P1_GATE_BLOOM_SOURCE:
			pass = vk_bloom_source_contract_validate( err, sizeof( err ) );
			ev = P1_EVIDENCE_STATIC;
			break;
		case P1_GATE_BLOOM_FIREFLY:
			/* Cvar alone is STATIC; GPU firefly is recorded via iq_lab stages. */
			pass = ( r_bloomFireflyClamp && r_bloomFireflyClamp->integer ) ? qtrue : qfalse;
			ev = P1_EVIDENCE_STATIC;
			break;
		case P1_GATE_NO_UNOWNED_HISTORY:
			pass = !vk_temporal_history_unowned_active();
			ev = P1_EVIDENCE_STATIC;
			break;
		case P1_GATE_GBUFFER_FULL_FIDELITY:
			pass = ( vk_gbuffer_quality_effective() >= 2 &&
				( !r_gbufferCompact || !r_gbufferCompact->integer ) ) ? qtrue : qfalse;
			ev = P1_EVIDENCE_STATIC;
			break;
		case P1_GATE_MSAA_POLICY:
			pass = !( ri.Cvar_VariableIntegerValue( "r_ext_multisample" ) > 0 &&
				r_oit && r_oit->integer >= 1 );
			ev = P1_EVIDENCE_STATIC;
			break;
		case P1_GATE_SMAA:
			pass = ( ri.Cvar_VariableIntegerValue( "r_ext_smaa" ) != 0 ||
				ri.Cvar_VariableIntegerValue( "r_aaMode" ) == 2 ) ? qtrue : qfalse;
			ev = P1_EVIDENCE_STATIC;
			break;
		case P1_GATE_TEXTURE_LOD:
			pass = ( atof( ri.Cvar_VariableString( "r_lodBias" ) ) >= -0.001 ) ? qtrue : qfalse;
			ev = P1_EVIDENCE_STATIC;
			break;
		case P1_GATE_VELOCITY:
		case P1_GATE_SPECULAR_STABILITY:
		case P1_GATE_DEFERRED_FORWARD_PARITY:
		case P1_GATE_LIGHTING_OWNERSHIP:
		case P1_GATE_CLUSTER_PARITY:
		case P1_GATE_EDGE_REFERENCE:
			/* Measured gates: PENDING until iq_lab records GPU_READBACK. */
			pass = qfalse;
			ev = P1_EVIDENCE_PENDING;
			break;
		default:
			pass = qfalse;
			ev = P1_EVIDENCE_NONE;
			break;
		}
		s_gateEvidence[i] = ev;
		if ( pass ) {
			s_p1GatePass[i]++;
		} else {
			s_p1GateFail[i]++;
		}
	}
}

rendererP1Evidence_t vk_renderer_p1_gate_evidence( rendererP1Gate_t gate )
{
	if ( gate < 0 || gate >= P1_GATE_COUNT ) {
		return P1_EVIDENCE_NONE;
	}
	return s_gateEvidence[gate];
}

void vk_renderer_p1_status_f( void )
{
	int i;
	rendererP1Level_t lvl;

	vk_renderer_p1_cert_refresh_static();
	EvaluateP1Gates();
	lvl = vk_renderer_p1_cert_level();

	ri.Printf( PRINT_ALL, "=== Renderer P1 certification (Phase 1.5 honest ladder) ===\n" );
	ri.Printf( PRINT_ALL,
		"LEVEL: %s\n"
		"  (PROFILE_CERTIFIED is the maximum from static/cvar checklist alone;\n"
		"   IMAGE_QUALITY_CERTIFIED requires iq_certify_core GPU evidence)\n",
		vk_renderer_p1_level_name( lvl ) );

	for ( i = 0; i < P1_GATE_COUNT; i++ ) {
		char err[64];
		qboolean latest = qfalse;
		const char *evName;

		switch ( (rendererP1Gate_t)i ) {
		case P1_GATE_BLOOM_FIREFLY:
			latest = ( r_bloomFireflyClamp && r_bloomFireflyClamp->integer ) ? qtrue : qfalse;
			break;
		case P1_GATE_GBUFFER_FULL_FIDELITY:
			latest = ( vk_gbuffer_quality_effective() >= 2 &&
				( !r_gbufferCompact || !r_gbufferCompact->integer ) ) ? qtrue : qfalse;
			break;
		case P1_GATE_MSAA_POLICY:
			latest = !( ri.Cvar_VariableIntegerValue( "r_ext_multisample" ) > 0 &&
				r_oit && r_oit->integer >= 1 );
			break;
		case P1_GATE_TEXTURE_LOD:
			latest = ( atof( ri.Cvar_VariableString( "r_lodBias" ) ) >= -0.001 ) ? qtrue : qfalse;
			break;
		case P1_GATE_BLOOM_SOURCE:
			latest = vk_bloom_source_contract_validate( err, sizeof( err ) );
			break;
		case P1_GATE_SMAA:
			latest = ( ri.Cvar_VariableIntegerValue( "r_ext_smaa" ) != 0 ||
				ri.Cvar_VariableIntegerValue( "r_aaMode" ) == 2 ) ? qtrue : qfalse;
			break;
		case P1_GATE_NO_UNOWNED_HISTORY:
			latest = !vk_temporal_history_unowned_active();
			break;
		case P1_GATE_VELOCITY:
		case P1_GATE_SPECULAR_STABILITY:
		case P1_GATE_DEFERRED_FORWARD_PARITY:
		case P1_GATE_LIGHTING_OWNERSHIP:
		case P1_GATE_CLUSTER_PARITY:
		case P1_GATE_EDGE_REFERENCE:
			latest = qfalse;
			s_gateEvidence[i] = P1_EVIDENCE_PENDING;
			break;
		default:
			latest = qfalse;
			break;
		}
		evName = vk_renderer_p1_evidence_name( s_gateEvidence[i] );
		ri.Printf( PRINT_ALL, "  %s %s evidence=%s\n",
			latest ? "PASS" : ( s_gateEvidence[i] == P1_EVIDENCE_PENDING ? "PEND" : "FAIL" ),
			P1GateName( (rendererP1Gate_t)i ), evName );
	}
	ri.Printf( PRINT_ALL,
		"stages: iq_certification_status | iq_certify_core | iq_lab_status\n" );
}

static void RendererP1Certify_f( void )
{
	vk_renderer_p1_status_f();
}

void vk_renderer_iq_p1_register( void )
{
	r_ghostIsolation = ri.Cvar_Get( "r_ghostIsolation", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_ghostIsolation, "0", "7", CV_INTEGER );
	ri.Cvar_SetDescription( r_ghostIsolation,
		"IQ P1-G ghost isolation: 0=all 1=noTAA 2=noSSR 3=noAO 4=noVol 5=fixedExp 6=noWeaponHist 7=none." );

	r_gbufferQuality = ri.Cvar_Get( "r_gbufferQuality", "2", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_gbufferQuality, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_gbufferQuality,
		"G-buffer quality: 0=compact 1=balanced 2=full-fidelity IQ reference. Pair with r_gbufferCompact 0." );
	ri.Cvar_SetGroup( r_gbufferQuality, CVG_RENDERER );
	r_ssrTemporal = ri.Cvar_Get( "r_ssrTemporal", "0", CVAR_ARCHIVE_ND );
	r_allowExperimentalTemporalSSR =
		ri.Cvar_Get( "r_allowExperimentalTemporalSSR", "0", CVAR_ARCHIVE_ND );

	r_bloomFireflyClamp = ri.Cvar_Get( "r_bloomFireflyClamp", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_bloomFireflyClamp, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_bloomFireflyClamp,
		"Clamp isolated HDR spikes on bloom extract only (does not blur SceneHDR)." );
	ri.Cvar_SetGroup( r_bloomFireflyClamp, CVG_RENDERER );

	r_bloomFireflyRatio = ri.Cvar_Get( "r_bloomFireflyRatio", "4.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_bloomFireflyRatio, "1.0", "32.0", CV_FLOAT );
	ri.Cvar_SetGroup( r_bloomFireflyRatio, CVG_RENDERER );

	r_bloomFireflyAbsolute = ri.Cvar_Get( "r_bloomFireflyAbsolute", "0.25", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_bloomFireflyAbsolute, "0.0", "16.0", CV_FLOAT );
	ri.Cvar_SetGroup( r_bloomFireflyAbsolute, CVG_RENDERER );

	r_bloomFireflyNeighborhood = ri.Cvar_Get( "r_bloomFireflyNeighborhood", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_bloomFireflyNeighborhood, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_bloomFireflyNeighborhood, "0=cross 1=3x3 median-ish 2=3x3 trimmed mean." );
	ri.Cvar_SetGroup( r_bloomFireflyNeighborhood, CVG_RENDERER );

	r_bloomFireflyDebug = ri.Cvar_Get( "r_bloomFireflyDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_bloomFireflyDebug, "0", "6", CV_INTEGER );

	r_bloomTemporalOrder = ri.Cvar_Get( "r_bloomTemporalOrder", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_bloomTemporalOrder, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_bloomTemporalOrder,
		"1=weapon-before-bloom (COLOR_PIPELINE / r_weaponBloomMode 1 path)." );
	ri.Cvar_SetGroup( r_bloomTemporalOrder, CVG_RENDERER );

	r_bloomGhostingDebug = ri.Cvar_Get( "r_bloomGhostingDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_bloomGhostingDebug, "0", "4", CV_INTEGER );

	r_bloomDebug = ri.Cvar_Get( "r_bloomDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_bloomDebug, "0", "10", CV_INTEGER );

	/* Aliases for user-facing specular AA names (map to existing r_pbr_specularAA*). */
	ri.Cvar_Get( "r_specularAA", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_Get( "r_specularAAVarianceScale", "0.5", CVAR_ARCHIVE_ND );
	ri.Cvar_Get( "r_specularAAMaxRoughness", "0.95", CVAR_ARCHIVE_ND );
	ri.Cvar_Get( "r_specularAADebug", "0", CVAR_CHEAT );

	vk_bloom_source_contract_register();
	vk_renderer_p1_cert_register();
	vk_iq_cert_geometry_register();
	vk_iq_lab_register();
	vk_renderer_p1_live_register();

	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "renderer_iq_profile_status", IQ_ProfileStatus_f );
		ri.Cmd_AddCommand( "renderer_iq_profile_validate", IQ_ProfileValidate_f );
		ri.Cmd_AddCommand( "renderer_iq_profile_apply", IQ_ProfileApply_f );
		ri.Cmd_AddCommand( "temporal_history_status", vk_temporal_history_status_f );
		ri.Cmd_AddCommand( "temporal_history_validate", vk_temporal_history_status_f );
		ri.Cmd_AddCommand( "ssr_temporal_status", SsrTemporalStatus_f );
		ri.Cmd_AddCommand( "ssr_temporal_validate", SsrTemporalValidate_f );
		ri.Cmd_AddCommand( "ghosting_isolation_status", GhostIsolationStatus_f );
		ri.Cmd_AddCommand( "bloom_pyramid_status", BloomPyramidStatus_f );
		ri.Cmd_AddCommand( "bloom_filter_status", BloomPyramidStatus_f );
		ri.Cmd_AddCommand( "bloom_ghosting_status", BloomGhostingStatus_f );
		ri.Cmd_AddCommand( "gbuffer_quality_status", GbufferQualityStatus_f );
		ri.Cmd_AddCommand( "gbuffer_memory_status", GbufferQualityStatus_f );
		ri.Cmd_AddCommand( "msaa_policy_status", MsaaPolicyStatus_f );
		ri.Cmd_AddCommand( "msaa_policy_validate", MsaaPolicyStatus_f );
		ri.Cmd_AddCommand( "renderer_p1_status", vk_renderer_p1_status_f );
		ri.Cmd_AddCommand( "renderer_p1_certify", RendererP1Certify_f );
		s_cmds = qtrue;
	}

	/* Seed history owners as registered (invalid until first use). */
	vk_temporal_history_note( HISTORY_BLOOM, qfalse, "no bloom temporal buffer" );
	vk_temporal_history_note( HISTORY_TAA, qfalse, "inactive" );
	vk_temporal_history_note( HISTORY_WEAPON, qfalse, "inactive" );
	vk_temporal_history_note( HISTORY_SSR, qfalse, "inactive" );
	vk_temporal_history_note( HISTORY_AO, qfalse, "inactive" );
	vk_temporal_history_note( HISTORY_VOLUMETRIC, qfalse, "inactive" );
	vk_temporal_history_note( HISTORY_EXPOSURE, qfalse, "inactive" );

	ri.Printf( PRINT_ALL,
		"[VK][IQ-P1] hub ready (honest multi-level cert; iq_certify_core for GPU evidence)\n" );
}

#endif /* USE_VULKAN */
