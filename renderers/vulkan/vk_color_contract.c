/*
===========================================================================
Color Pipeline Reconstruction — Phase 1 authoritative color contract.
Enforces space + stage order for scene-linear HDR + WBOIT production OIT.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_color_contract.h"
#include "vk_hdr_pipeline.h"

static cvar_t *r_colorContractDebug;
static qboolean s_cmds;

typedef struct {
	char writer[48];
	vkColorSpace_t space;
	vkAlphaEncoding_t alpha;
	qboolean noted;
} vkColorStageState_t;

static vkColorStageState_t s_stages[VK_COLOR_STAGE_COUNT];
static uint32_t s_validateFails;
static uint32_t s_spaceMismatches;
static uint32_t s_orderViolations;
static uint32_t s_frameOrderViolations;
static uint32_t s_frameSpaceMismatches;

const char *vk_color_space_name( vkColorSpace_t space )
{
	switch ( space ) {
	case VK_COLOR_SPACE_TEXTURE_SRGB: return "TEXTURE_SRGB";
	case VK_COLOR_SPACE_TEXTURE_LINEAR: return "TEXTURE_LINEAR";
	case VK_COLOR_SPACE_SCENE_LINEAR_HDR: return "SCENE_LINEAR_HDR";
	case VK_COLOR_SPACE_PREEXPOSED_SCENE_LINEAR_HDR: return "PREEXPOSED_SCENE_LINEAR_HDR";
	case VK_COLOR_SPACE_DISPLAY_LINEAR: return "DISPLAY_LINEAR";
	case VK_COLOR_SPACE_DISPLAY_ENCODED: return "DISPLAY_ENCODED";
	default: return "UNKNOWN";
	}
}

const char *vk_color_stage_name( vkColorPipelineStage_t stage )
{
	switch ( stage ) {
	case VK_COLOR_STAGE_TEXTURE_DECODE: return "texture_decode";
	case VK_COLOR_STAGE_MATERIAL_EVAL: return "material_eval";
	case VK_COLOR_STAGE_OPAQUE_LIGHTING: return "opaque_lighting";
	case VK_COLOR_STAGE_SKY_ATMOSPHERE: return "sky_atmosphere";
	case VK_COLOR_STAGE_OPAQUE_HDR_COMPOSITE: return "opaque_hdr_composite";
	case VK_COLOR_STAGE_TRANSPARENT_LIGHTING: return "transparent_lighting";
	case VK_COLOR_STAGE_OIT_ACCUM: return "oit_accum";
	case VK_COLOR_STAGE_OIT_RESOLVE: return "oit_resolve";
	case VK_COLOR_STAGE_REFRACTION: return "refraction";
	case VK_COLOR_STAGE_WEAPON_HDR: return "weapon_hdr";
	case VK_COLOR_STAGE_VOLUMETRIC: return "volumetric";
	case VK_COLOR_STAGE_BLOOM: return "bloom";
	case VK_COLOR_STAGE_EXPOSURE: return "exposure";
	case VK_COLOR_STAGE_TONEMAP: return "tonemap";
	case VK_COLOR_STAGE_COLOR_GRADE: return "color_grade";
	case VK_COLOR_STAGE_DISPLAY_TRANSFORM: return "display_transform";
	case VK_COLOR_STAGE_UI: return "ui";
	default: return "unknown";
	}
}

const char *vk_alpha_encoding_name( vkAlphaEncoding_t alpha )
{
	switch ( alpha ) {
	case VK_ALPHA_ENCODING_OPAQUE: return "opaque";
	case VK_ALPHA_ENCODING_STRAIGHT: return "straight";
	case VK_ALPHA_ENCODING_PREMULTIPLIED: return "premultiplied";
	case VK_ALPHA_ENCODING_COVERAGE: return "coverage";
	default: return "unknown";
	}
}

vkColorSpace_t vk_color_stage_expected_space( vkColorPipelineStage_t stage )
{
	switch ( stage ) {
	case VK_COLOR_STAGE_TEXTURE_DECODE:
		return VK_COLOR_SPACE_TEXTURE_LINEAR;
	case VK_COLOR_STAGE_MATERIAL_EVAL:
	case VK_COLOR_STAGE_OPAQUE_LIGHTING:
	case VK_COLOR_STAGE_SKY_ATMOSPHERE:
	case VK_COLOR_STAGE_OPAQUE_HDR_COMPOSITE:
	case VK_COLOR_STAGE_TRANSPARENT_LIGHTING:
	case VK_COLOR_STAGE_OIT_ACCUM:
	case VK_COLOR_STAGE_OIT_RESOLVE:
	case VK_COLOR_STAGE_REFRACTION:
	case VK_COLOR_STAGE_WEAPON_HDR:
	case VK_COLOR_STAGE_VOLUMETRIC:
	case VK_COLOR_STAGE_BLOOM:
		return VK_COLOR_SPACE_SCENE_LINEAR_HDR;
	case VK_COLOR_STAGE_EXPOSURE:
		return VK_COLOR_SPACE_PREEXPOSED_SCENE_LINEAR_HDR;
	case VK_COLOR_STAGE_TONEMAP:
	case VK_COLOR_STAGE_COLOR_GRADE:
		return VK_COLOR_SPACE_DISPLAY_LINEAR;
	case VK_COLOR_STAGE_DISPLAY_TRANSFORM:
	case VK_COLOR_STAGE_UI:
		return VK_COLOR_SPACE_DISPLAY_ENCODED;
	default:
		return VK_COLOR_SPACE_SCENE_LINEAR_HDR;
	}
}

vkAlphaEncoding_t vk_color_stage_expected_alpha( vkColorPipelineStage_t stage )
{
	switch ( stage ) {
	case VK_COLOR_STAGE_OIT_ACCUM:
		return VK_ALPHA_ENCODING_PREMULTIPLIED;
	case VK_COLOR_STAGE_OIT_RESOLVE:
		return VK_ALPHA_ENCODING_COVERAGE;
	case VK_COLOR_STAGE_UI:
		return VK_ALPHA_ENCODING_STRAIGHT;
	case VK_COLOR_STAGE_TRANSPARENT_LIGHTING:
		return VK_ALPHA_ENCODING_STRAIGHT;
	default:
		return VK_ALPHA_ENCODING_OPAQUE;
	}
}

qboolean vk_color_contract_wboit_is_production( void )
{
	return ( !r_oit || r_oit->integer <= 1 ) ? qtrue : qfalse;
}

void vk_color_contract_begin_frame( void )
{
	Com_Memset( s_stages, 0, sizeof( s_stages ) );
	s_frameOrderViolations = 0;
	s_frameSpaceMismatches = 0;
}

void vk_color_contract_note_stage( vkColorPipelineStage_t stage, const char *passName,
	vkColorSpace_t space, vkAlphaEncoding_t alpha )
{
	vkColorStageState_t *st;
	vkColorSpace_t expect;
	int i;

	if ( stage < 0 || stage >= VK_COLOR_STAGE_COUNT || !passName || !passName[0] ) {
		return;
	}

	for ( i = (int)stage + 1; i < (int)VK_COLOR_STAGE_COUNT; i++ ) {
		if ( s_stages[i].noted ) {
			s_orderViolations++;
			s_frameOrderViolations++;
			if ( r_colorContractDebug && r_colorContractDebug->integer ) {
				ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
					"[VK][color] order violation: %s after already-noted %s\n" S_COLOR_WHITE,
					vk_color_stage_name( stage ),
					vk_color_stage_name( (vkColorPipelineStage_t)i ) );
			}
			break;
		}
	}

	st = &s_stages[stage];
	Q_strncpyz( st->writer, passName, sizeof( st->writer ) );
	st->space = space;
	st->alpha = alpha;
	st->noted = qtrue;

	expect = vk_color_stage_expected_space( stage );
	if ( space != expect ) {
		s_spaceMismatches++;
		s_frameSpaceMismatches++;
		if ( r_colorContractDebug && r_colorContractDebug->integer ) {
			ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
				"[VK][color] space mismatch stage=%s got=%s expect=%s pass=%s\n" S_COLOR_WHITE,
				vk_color_stage_name( stage ),
				vk_color_space_name( space ),
				vk_color_space_name( expect ),
				passName );
		}
	}

	if ( stage == VK_COLOR_STAGE_OPAQUE_HDR_COMPOSITE ||
		stage == VK_COLOR_STAGE_OIT_RESOLVE ||
		stage == VK_COLOR_STAGE_WEAPON_HDR ) {
		vk_hdr_pipeline_note_stage( VK_HDR_STAGE_SCENE, passName );
	} else if ( stage == VK_COLOR_STAGE_BLOOM ) {
		vk_hdr_pipeline_note_stage( VK_HDR_STAGE_BLOOM, passName );
	} else if ( stage == VK_COLOR_STAGE_EXPOSURE ) {
		vk_hdr_pipeline_note_stage( VK_HDR_STAGE_EXPOSURE, passName );
	} else if ( stage == VK_COLOR_STAGE_TONEMAP ) {
		vk_hdr_pipeline_note_stage( VK_HDR_STAGE_TONEMAP, passName );
	} else if ( stage == VK_COLOR_STAGE_DISPLAY_TRANSFORM ) {
		vk_hdr_pipeline_note_stage( VK_HDR_STAGE_GAMMA, passName );
	}
}

qboolean vk_color_contract_validate( char *errBuf, int errBufSize )
{
	int i;
	qboolean ok = qtrue;

	if ( errBuf && errBufSize > 0 ) {
		errBuf[0] = '\0';
	}

	if ( vk_color_stage_expected_space( VK_COLOR_STAGE_OIT_ACCUM ) != VK_COLOR_SPACE_SCENE_LINEAR_HDR ||
		vk_color_stage_expected_space( VK_COLOR_STAGE_OIT_RESOLVE ) != VK_COLOR_SPACE_SCENE_LINEAR_HDR ||
		vk_color_stage_expected_space( VK_COLOR_STAGE_BLOOM ) != VK_COLOR_SPACE_SCENE_LINEAR_HDR ||
		vk_color_stage_expected_space( VK_COLOR_STAGE_EXPOSURE ) != VK_COLOR_SPACE_PREEXPOSED_SCENE_LINEAR_HDR ||
		vk_color_stage_expected_space( VK_COLOR_STAGE_TONEMAP ) != VK_COLOR_SPACE_DISPLAY_LINEAR ||
		vk_color_stage_expected_space( VK_COLOR_STAGE_DISPLAY_TRANSFORM ) != VK_COLOR_SPACE_DISPLAY_ENCODED ) {
		ok = qfalse;
		s_validateFails++;
		if ( errBuf && errBufSize > 0 ) {
			Q_strncpyz( errBuf, "static expected-space table inconsistent", errBufSize );
		}
		return qfalse;
	}

	for ( i = 0; i < (int)VK_COLOR_STAGE_COUNT; i++ ) {
		if ( !s_stages[i].noted ) {
			continue;
		}
		if ( s_stages[i].space != vk_color_stage_expected_space( (vkColorPipelineStage_t)i ) ) {
			ok = qfalse;
			if ( errBuf && errBufSize > 0 && !errBuf[0] ) {
				Com_sprintf( errBuf, errBufSize, "space mismatch on %s",
					vk_color_stage_name( (vkColorPipelineStage_t)i ) );
			}
		}
	}

	if ( s_frameOrderViolations > 0 ) {
		ok = qfalse;
		if ( errBuf && errBufSize > 0 && !errBuf[0] ) {
			Q_strncpyz( errBuf, "stage order violation this frame", errBufSize );
		}
	}

	if ( !ok ) {
		s_validateFails++;
	}
	return ok;
}

static void VK_Color_PipelineStatus_f( void )
{
	int i;
	char err[128];
	const qboolean valid = vk_color_contract_validate( err, sizeof( err ) );
	const int oit = r_oit ? r_oit->integer : 0;

	ri.Printf( PRINT_ALL, "======== Color Pipeline Contract (Phase 1) ========\n" );
	ri.Printf( PRINT_ALL,
		"order: texture→material→opaqueLit→sky→opaqueHDR→xparentLit→oitAccum→oitResolve→\n"
		"       refract→weapon→volumetric→bloom→exposure→tonemap→grade→display→ui\n" );
	ri.Printf( PRINT_ALL,
		"spaces: TEXTURE_SRGB|LINEAR → SCENE_LINEAR_HDR → PREEXPOSED → DISPLAY_LINEAR → DISPLAY_ENCODED\n" );
	ri.Printf( PRINT_ALL, "OIT production=%s (r_oit=%d: 0=off 1=WBOIT production 2=MBOIT experimental)\n",
		( oit == 1 ) ? "WBOIT" : ( oit == 2 ) ? "MBOIT(experimental)" : "off",
		oit );
	ri.Printf( PRINT_ALL,
		"validate=%s fails=%u spaceMismatch=%u orderViol=%u (frame space=%u order=%u)\n",
		valid ? "PASS" : "FAIL", s_validateFails, s_spaceMismatches, s_orderViolations,
		s_frameSpaceMismatches, s_frameOrderViolations );
	if ( !valid && err[0] ) {
		ri.Printf( PRINT_ALL, "  reason: %s\n", err );
	}
	for ( i = 0; i < (int)VK_COLOR_STAGE_COUNT; i++ ) {
		const vkColorPipelineStage_t st = (vkColorPipelineStage_t)i;
		ri.Printf( PRINT_ALL, "  %2d %-22s expect=%-28s alpha=%-12s last=%s\n",
			i,
			vk_color_stage_name( st ),
			vk_color_space_name( vk_color_stage_expected_space( st ) ),
			vk_alpha_encoding_name( vk_color_stage_expected_alpha( st ) ),
			s_stages[i].noted ? s_stages[i].writer : "(none)" );
	}
	ri.Printf( PRINT_ALL, "See docs/COLOR_PIPELINE.md\n" );
	ri.Printf( PRINT_ALL, "===================================================\n" );
}

static void VK_Color_PipelineValidate_f( void )
{
	char err[160];
	if ( vk_color_contract_validate( err, sizeof( err ) ) ) {
		ri.Printf( PRINT_ALL, "color_pipeline_validate: PASS\n" );
	} else {
		ri.Printf( PRINT_ALL, "color_pipeline_validate: FAIL (%s)\n", err[0] ? err : "unknown" );
	}
}

void vk_color_contract_register( void )
{
	r_colorContractDebug = ri.Cvar_Get( "r_colorContractDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_colorContractDebug, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_colorContractDebug,
		"Color contract debug: 1 warn on space mismatch / order, 2 verbose." );
	ri.Cvar_SetGroup( r_colorContractDebug, CVG_RENDERER );

	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "color_pipeline_status", VK_Color_PipelineStatus_f );
		ri.Cmd_AddCommand( "color_pipeline_validate", VK_Color_PipelineValidate_f );
		s_cmds = qtrue;
		ri.Printf( PRINT_ALL,
			"[VK][color] Phase 1 contract ready (color_pipeline_status / color_pipeline_validate)\n" );
	}
}
