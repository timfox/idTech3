/*
===========================================================================
Phase 1.6 — evidence persistence + dependency invalidation.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_renderer_p1_evidence.h"
#include "vk_renderer_p1_cert.h"
#include "vk_renderer_p1_thresholds.h"
#include "vk_renderer_iq_p1.h"
#include <string.h>


static qboolean s_cmds;

uint32_t vk_renderer_p1_profile_hash( void )
{
	uint32_t h = 0x51F10001u;
	h ^= (uint32_t)ri.Cvar_VariableIntegerValue( "r_oit" ) * 3u;
	h ^= (uint32_t)ri.Cvar_VariableIntegerValue( "r_taa" ) * 5u;
	h ^= (uint32_t)ri.Cvar_VariableIntegerValue( "r_bloom" ) * 7u;
	h ^= (uint32_t)ri.Cvar_VariableIntegerValue( "r_gbufferQuality" ) * 11u;
	h ^= (uint32_t)ri.Cvar_VariableIntegerValue( "r_gbufferCompact" ) * 13u;
	h ^= (uint32_t)ri.Cvar_VariableIntegerValue( "r_bloomFireflyClamp" ) * 17u;
	h ^= (uint32_t)ri.Cvar_VariableIntegerValue( "r_aaMode" ) * 19u;
	return h;
}

uint32_t vk_renderer_p1_evidence_build_id( void )
{
	return (uint32_t)tr.frameCount ^ 0x1D6E0001u;
}

void vk_renderer_p1_evidence_invalidate_dep( const char *depToken, const char *reason )
{
	char msg[192];
	Com_sprintf( msg, sizeof( msg ), "invalidate dep=%s: %s",
		depToken ? depToken : "?", reason ? reason : "" );

	if ( !depToken ) {
		vk_renderer_p1_cert_invalidate_all( msg );
		return;
	}
	if ( Q_stristr( depToken, "bloom" ) ) {
		vk_renderer_p1_cert_stage_skip( P1_CERT_STAGE_BLOOM_SOURCE, msg );
		vk_renderer_p1_cert_stage_skip( P1_CERT_STAGE_BLOOM_FIREFLY, msg );
		vk_renderer_p1_cert_stage_skip( P1_CERT_STAGE_BLOOM_PYRAMID, msg );
	}
	if ( Q_stristr( depToken, "velocity" ) || Q_stristr( depToken, "temporal" ) ) {
		vk_renderer_p1_cert_stage_skip( P1_CERT_STAGE_VELOCITY, msg );
		vk_renderer_p1_cert_stage_skip( P1_CERT_STAGE_TEMPORAL_HISTORY, msg );
		vk_renderer_p1_cert_stage_skip( P1_CERT_STAGE_TEMPORAL_RESET, msg );
		vk_renderer_p1_cert_stage_skip( P1_CERT_STAGE_GHOSTING, msg );
	}
	if ( Q_stristr( depToken, "gbuffer" ) ) {
		vk_renderer_p1_cert_stage_skip( P1_CERT_STAGE_GBUFFER_QUANT, msg );
		vk_renderer_p1_cert_stage_skip( P1_CERT_STAGE_MATERIAL_DECODE, msg );
		vk_renderer_p1_cert_stage_skip( P1_CERT_STAGE_LIGHTING_PARITY, msg );
	}
	if ( Q_stristr( depToken, "cluster" ) ) {
		vk_renderer_p1_cert_stage_skip( P1_CERT_STAGE_CLUSTER_PARITY, msg );
		vk_renderer_p1_cert_stage_skip( P1_CERT_STAGE_LIGHTING_PARITY, msg );
	}
	if ( Q_stristr( depToken, "smaa" ) || Q_stristr( depToken, "edge" ) ) {
		vk_renderer_p1_cert_stage_skip( P1_CERT_STAGE_EDGE, msg );
		vk_renderer_p1_cert_stage_skip( P1_CERT_STAGE_SMAA, msg );
	}
	if ( Q_stristr( depToken, "threshold" ) ) {
		vk_renderer_p1_cert_invalidate_all( msg );
	}
	ri.Printf( PRINT_ALL, "iq_evidence_invalidate: %s\n", msg );
}

void vk_renderer_p1_evidence_fill_header( char *buf, int bufSize, int *outOff )
{
	int off = outOff ? *outOff : 0;
	off += Com_sprintf( buf + off, bufSize - off,
		"  \"buildId\": %u,\n"
		"  \"profileHash\": %u,\n"
		"  \"thresholdHash\": %u,\n"
		"  \"device\": \"%s\",\n"
		"  \"extent\": [%d, %d],\n",
		vk_renderer_p1_evidence_build_id(),
		vk_renderer_p1_profile_hash(),
		vk_renderer_p1_thresholds_hash(),
		vk.device ? "local-gpu" : "none",
		glConfig.vidWidth, glConfig.vidHeight );
	if ( outOff ) {
		*outOff = off;
	}
}

static void P1_Evidence_Invalidate_f( void )
{
	vk_renderer_p1_evidence_invalidate_dep(
		( ri.Cmd_Argc() >= 2 ) ? ri.Cmd_Argv( 1 ) : "all",
		( ri.Cmd_Argc() >= 3 ) ? ri.Cmd_Argv( 2 ) : "operator" );
}

void vk_renderer_p1_evidence_register( void )
{
	if ( s_cmds ) {
		return;
	}
	s_cmds = qtrue;
	if ( ri.Cmd_AddCommand ) {
		ri.Cmd_AddCommand( "iq_evidence_invalidate", P1_Evidence_Invalidate_f );
	}
}

