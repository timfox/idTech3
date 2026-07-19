/*
===========================================================================
Layered antialiasing policy: r_aaMode drives SMAA / FXAA / Temporal Reconstruction.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_aa_policy.h"

cvar_t *r_aaMode;
cvar_t *r_temporalHistoryWeight;
cvar_t *r_temporalVarianceClip;
cvar_t *r_temporalDisocclusion;
cvar_t *r_temporalReactiveMask;
cvar_t *r_temporalSmaaCleanup;
cvar_t *r_debugMotionVectors;
cvar_t *r_debugHistoryRejection;

static qboolean s_want_temporal_cleanup_smaa;
static qboolean s_want_smaa_t2x;
static int s_last_applied_mode = -1;

static void vk_aa_set_int( cvar_t *cv, const char *name, int value )
{
	char buf[16];

	if ( !cv || !name ) {
		return;
	}
	if ( cv->integer == value ) {
		return;
	}
	Com_sprintf( buf, sizeof( buf ), "%d", value );
	ri.Cvar_Set( name, buf );
	cv->integer = value;
	cv->modified = qtrue;
}

static void vk_aa_set_float( cvar_t *cv, const char *name, float value )
{
	char buf[32];

	if ( !cv || !name ) {
		return;
	}
	if ( cv->value == value ) {
		return;
	}
	Com_sprintf( buf, sizeof( buf ), "%g", value );
	ri.Cvar_Set( name, buf );
	cv->value = value;
	cv->modified = qtrue;
}

void vk_aa_policy_register_cvars( void )
{
	r_aaMode = ri.Cvar_Get( "r_aaMode", "2", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_aaMode, "0", "6", CV_INTEGER );
	ri.Cvar_SetDescription( r_aaMode,
		"Presentation AA policy:\n"
		" 0=none\n"
		" 1=FXAA\n"
		" 2=SMAA 1x (default, sharp native baseline)\n"
		" 3=SMAA T2x scaffold (SMAA + light temporal)\n"
		" 4=Temporal Reconstruction\n"
		" 5=Temporal Reconstruction + SMAA cleanup\n"
		" 6=supersampled reference (r_ext_supersample)\n"
		"Applies r_ext_smaa / r_ext_fxaa / r_taa. Latched; vid_restart for SMAA/FXAA." );
	ri.Cvar_SetGroup( r_aaMode, CVG_RENDERER );

	r_temporalHistoryWeight = ri.Cvar_Get( "r_temporalHistoryWeight", "0.80", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_temporalHistoryWeight, "0", "0.95", CV_FLOAT );
	ri.Cvar_SetDescription( r_temporalHistoryWeight,
		"Max history weight for Temporal Reconstruction (scales stationary feedback). Default 0.80." );
	ri.Cvar_SetGroup( r_temporalHistoryWeight, CVG_RENDERER );

	r_temporalVarianceClip = ri.Cvar_Get( "r_temporalVarianceClip", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_temporalVarianceClip, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_temporalVarianceClip,
		"YCoCg variance clipping in Temporal Reconstruction (1=on)." );
	ri.Cvar_SetGroup( r_temporalVarianceClip, CVG_RENDERER );

	r_temporalDisocclusion = ri.Cvar_Get( "r_temporalDisocclusion", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_temporalDisocclusion, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_temporalDisocclusion,
		"Depth-based disocclusion rejection for Temporal Reconstruction (1=on)." );
	ri.Cvar_SetGroup( r_temporalDisocclusion, CVG_RENDERER );

	r_temporalReactiveMask = ri.Cvar_Get( "r_temporalReactiveMask", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_temporalReactiveMask, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_temporalReactiveMask,
		"Prefer current frame for reactive pixels (near/weapon, fast motion, transparent heuristics)." );
	ri.Cvar_SetGroup( r_temporalReactiveMask, CVG_RENDERER );

	r_temporalSmaaCleanup = ri.Cvar_Get( "r_temporalSmaaCleanup", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_temporalSmaaCleanup, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_temporalSmaaCleanup,
		"When 1 (or r_aaMode 5), run light SMAA after Temporal Reconstruction." );
	ri.Cvar_SetGroup( r_temporalSmaaCleanup, CVG_RENDERER );

	r_debugMotionVectors = ri.Cvar_Get( "r_debugMotionVectors", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_debugMotionVectors, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_debugMotionVectors,
		"Overlay motion vectors in Temporal Reconstruction resolve (debug)." );
	ri.Cvar_SetGroup( r_debugMotionVectors, CVG_RENDERER );

	r_debugHistoryRejection = ri.Cvar_Get( "r_debugHistoryRejection", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_debugHistoryRejection, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_debugHistoryRejection,
		"Color-code history rejection: green=accept, red=depth, blue=normal/proxy, yellow=reactive, "
		"magenta=invalid MV, cyan=luma, white=cut/reset." );
	ri.Cvar_SetGroup( r_debugHistoryRejection, CVG_RENDERER );
}

void vk_aa_policy_apply( void )
{
	int mode;

	if ( !r_aaMode ) {
		return;
	}

	mode = r_aaMode->integer;
	if ( mode < 0 || mode > 6 ) {
		mode = 2;
	}

	s_want_temporal_cleanup_smaa = qfalse;
	s_want_smaa_t2x = qfalse;

	switch ( mode ) {
	case 0: /* none */
		vk_aa_set_int( r_ext_smaa, "r_ext_smaa", 0 );
		vk_aa_set_int( r_ext_fxaa, "r_ext_fxaa", 0 );
		vk_aa_set_int( r_taa, "r_taa", 0 );
		vk_aa_set_int( r_ext_supersample, "r_ext_supersample", 0 );
		vk_aa_set_int( r_temporalSmaaCleanup, "r_temporalSmaaCleanup", 0 );
		break;
	case 1: /* FXAA */
		vk_aa_set_int( r_ext_smaa, "r_ext_smaa", 0 );
		vk_aa_set_int( r_ext_fxaa, "r_ext_fxaa", 1 );
		vk_aa_set_int( r_taa, "r_taa", 0 );
		vk_aa_set_int( r_ext_supersample, "r_ext_supersample", 0 );
		vk_aa_set_int( r_postAaAfterBloom, "r_postAaAfterBloom", 1 );
		break;
	case 2: /* SMAA 1x — shipping default */
		vk_aa_set_int( r_ext_smaa, "r_ext_smaa", 1 );
		vk_aa_set_int( r_ext_fxaa, "r_ext_fxaa", 0 );
		vk_aa_set_int( r_taa, "r_taa", 0 );
		vk_aa_set_int( r_ext_supersample, "r_ext_supersample", 0 );
		vk_aa_set_int( r_postAaAfterBloom, "r_postAaAfterBloom", 1 );
		if ( r_smaa_preset && r_smaa_preset->integer <= 0 ) {
			vk_aa_set_int( r_smaa_preset, "r_smaa_preset", 3 );
		}
		break;
	case 3: /* SMAA T2x scaffold: SMAA + light temporal */
		vk_aa_set_int( r_ext_smaa, "r_ext_smaa", 1 );
		vk_aa_set_int( r_ext_fxaa, "r_ext_fxaa", 0 );
		vk_aa_set_int( r_taa, "r_taa", 1 );
		vk_aa_set_int( r_taaMotionVectors, "r_taaMotionVectors", 1 );
		vk_aa_set_int( r_ext_supersample, "r_ext_supersample", 0 );
		vk_aa_set_int( r_postAaAfterBloom, "r_postAaAfterBloom", 1 );
		vk_aa_set_float( r_temporalHistoryWeight, "r_temporalHistoryWeight", 0.55f );
		s_want_smaa_t2x = qtrue;
		break;
	case 4: /* Temporal Reconstruction */
		vk_aa_set_int( r_ext_smaa, "r_ext_smaa", 0 );
		vk_aa_set_int( r_ext_fxaa, "r_ext_fxaa", 0 );
		vk_aa_set_int( r_taa, "r_taa", 1 );
		vk_aa_set_int( r_taaMotionVectors, "r_taaMotionVectors", 1 );
		vk_aa_set_int( r_ext_supersample, "r_ext_supersample", 0 );
		vk_aa_set_int( r_temporalSmaaCleanup, "r_temporalSmaaCleanup", 0 );
		break;
	case 5: /* Temporal Reconstruction + SMAA cleanup */
		vk_aa_set_int( r_ext_smaa, "r_ext_smaa", 1 );
		vk_aa_set_int( r_ext_fxaa, "r_ext_fxaa", 0 );
		vk_aa_set_int( r_taa, "r_taa", 1 );
		vk_aa_set_int( r_taaMotionVectors, "r_taaMotionVectors", 1 );
		vk_aa_set_int( r_ext_supersample, "r_ext_supersample", 0 );
		/* Spatial AA after bloom/fog, then temporal; cleanup flag for post-TAA light pass later. */
		vk_aa_set_int( r_postAaAfterBloom, "r_postAaAfterBloom", 0 );
		vk_aa_set_int( r_temporalSmaaCleanup, "r_temporalSmaaCleanup", 1 );
		s_want_temporal_cleanup_smaa = qtrue;
		break;
	case 6: /* supersampled reference */
		vk_aa_set_int( r_ext_smaa, "r_ext_smaa", 0 );
		vk_aa_set_int( r_ext_fxaa, "r_ext_fxaa", 0 );
		vk_aa_set_int( r_taa, "r_taa", 0 );
		vk_aa_set_int( r_ext_supersample, "r_ext_supersample", 1 );
		break;
	default:
		break;
	}

	if ( mode != s_last_applied_mode ) {
		static const char *names[] = {
			"none", "FXAA", "SMAA 1x", "SMAA T2x", "Temporal Reconstruction",
			"Temporal Reconstruction + SMAA cleanup", "supersample"
		};
		ri.Printf( PRINT_ALL, "[VK][AA] r_aaMode %d (%s)\n", mode, names[mode] );
		s_last_applied_mode = mode;
	}
}

qboolean vk_aa_policy_wants_temporal_cleanup_smaa( void )
{
	return ( s_want_temporal_cleanup_smaa ||
		( r_temporalSmaaCleanup && r_temporalSmaaCleanup->integer ) ) ? qtrue : qfalse;
}

qboolean vk_aa_policy_wants_smaa_t2x( void )
{
	return s_want_smaa_t2x;
}
