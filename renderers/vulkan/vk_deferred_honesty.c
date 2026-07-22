/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Deferred Honesty — eligibility, classic translation, status (HYBRID_ADDITIVE_DEFERRED).
===========================================================================
*/

#include "tr_local.h"
#include "tr_common.h"
#include "vk_deferred_honesty.h"
#include "vk_deferred_gbuffer.h"
#include "vk_transparency_route.h"
#include "vk_render_path.h"
#include "tr_render_mode_vk.h"
#include "vk.h"

cvar_t *r_deferredArchitecture;
cvar_t *r_deferredCompositeMode;
cvar_t *r_deferredEligibilityDebug;
cvar_t *r_gbufferInvalidPolicy;

typedef struct {
	uint32_t opaqueSurfaces;
	uint32_t deferredEligibleFull;
	uint32_t deferredEligibleApprox;
	uint32_t deferredExported;
	uint32_t forwardFallback;
	uint32_t unsupported;
	uint32_t defaultGBuffer;
	uint32_t litSceneAsBase;
	uint32_t validNormals;
	uint32_t validMaterial;
	uint32_t pbrNative;
	uint32_t classicTranslated;
	uint32_t reasonCounts[DEFERRED_REASON_COUNT];
} deferredHonestyFrame_t;

static deferredHonestyFrame_t s_frame;
static qboolean s_registered;

const char *R_DeferredArchitecture_Name( deferredArchitecture_t arch )
{
	switch ( arch ) {
	case DEFERRED_ARCH_ADDITIVE_HYBRID: return "HYBRID_ADDITIVE_DEFERRED";
	case DEFERRED_ARCH_MIXED_MATERIAL: return "MIXED_MATERIAL_DEFERRED";
	case DEFERRED_ARCH_STRICT_VALIDATION: return "STRICT_DEFERRED_VALIDATION";
	case DEFERRED_ARCH_COMPARE: return "DEFERRED_COMPARISON";
	default: return "UNKNOWN";
	}
}

qboolean R_DeferredMixedMaterialWanted( void )
{
	/* Arch 1+ use true unlit G-buffer + deferred static/dynamic for eligible pixels.
	 * Arch 3 (compare) also drives the mixed export on the deferred side. */
	if ( !r_deferredArchitecture ) {
		return qfalse;
	}
	return ( r_deferredArchitecture->integer >= DEFERRED_ARCH_MIXED_MATERIAL ) ? qtrue : qfalse;
}

qboolean R_DeferredStrictValidationWanted( void )
{
	return ( r_deferredArchitecture &&
		r_deferredArchitecture->integer == DEFERRED_ARCH_STRICT_VALIDATION ) ? qtrue : qfalse;
}

const char *R_DeferredCompositeMode_Name( deferredCompositeMode_t mode )
{
	switch ( mode ) {
	case DEFERRED_COMPOSITE_ADDITIVE_HYBRID: return "additive_hybrid (SceneBaseLit + DeferredDynamic)";
	case DEFERRED_COMPOSITE_FULL_REPLACE: return "full_replace (eligible pixels)";
	case DEFERRED_COMPOSITE_SIDE_BY_SIDE: return "debug_side_by_side";
	case DEFERRED_COMPOSITE_MATERIAL_VALIDATE: return "material_data_validation";
	default: return "unknown";
	}
}

const char *R_DeferredEligibility_Name( deferredEligibility_t elig )
{
	switch ( elig ) {
	case DEFERRED_ELIGIBLE_FULL: return "ELIGIBLE_FULL";
	case DEFERRED_ELIGIBLE_APPROXIMATE: return "ELIGIBLE_APPROXIMATE";
	case DEFERRED_FORWARD_FALLBACK: return "FORWARD_FALLBACK";
	case DEFERRED_UNSUPPORTED: return "UNSUPPORTED";
	case DEFERRED_DEBUG_FORCED: return "DEBUG_FORCED";
	default: return "UNKNOWN";
	}
}

const char *R_DeferredEligibilityReason_Name( deferredEligibilityReason_t reason )
{
	switch ( reason ) {
	case DEFERRED_REASON_NONE: return "NONE";
	case DEFERRED_REASON_NO_BASE_COLOR_EXPORT: return "NO_BASE_COLOR_EXPORT";
	case DEFERRED_REASON_NO_NORMAL_EXPORT: return "NO_NORMAL_EXPORT";
	case DEFERRED_REASON_NO_MATERIAL_RESPONSE: return "NO_MATERIAL_RESPONSE";
	case DEFERRED_REASON_MULTISTAGE_CLASSIC_SHADER: return "MULTISTAGE_CLASSIC_SHADER";
	case DEFERRED_REASON_BLENDED_SURFACE: return "BLENDED_SURFACE";
	case DEFERRED_REASON_DEFORMED_SURFACE: return "DEFORMED_SURFACE";
	case DEFERRED_REASON_PORTAL_OR_MIRROR: return "PORTAL_OR_MIRROR";
	case DEFERRED_REASON_SPECIAL_ENVIRONMENT_STAGE: return "SPECIAL_ENVIRONMENT_STAGE";
	case DEFERRED_REASON_UNSUPPORTED_TCMOD: return "UNSUPPORTED_TCMOD";
	case DEFERRED_REASON_UNSUPPORTED_ANIMATION: return "UNSUPPORTED_ANIMATION";
	case DEFERRED_REASON_TRANSMISSION_OR_REFRACTION: return "TRANSMISSION_OR_REFRACTION";
	case DEFERRED_REASON_FORWARD_ONLY_POLICY: return "FORWARD_ONLY_POLICY";
	case DEFERRED_REASON_SKY: return "SKY";
	case DEFERRED_REASON_WEAPON_OR_UI: return "WEAPON_OR_UI";
	case DEFERRED_REASON_TRANSPARENT: return "TRANSPARENT";
	case DEFERRED_REASON_CLASSIC_OR_MODE0: return "CLASSIC_OR_MODE0";
	case DEFERRED_REASON_PATH_NOT_READY: return "PATH_NOT_READY";
	case DEFERRED_REASON_PBR_NATIVE: return "PBR_NATIVE";
	case DEFERRED_REASON_CLASSIC_TRANSLATED: return "CLASSIC_TRANSLATED";
	default: return "UNKNOWN";
	}
}

void R_DeferredEligibility_DebugColor( deferredEligibility_t elig, float outRgb[3] )
{
	switch ( elig ) {
	case DEFERRED_ELIGIBLE_FULL:
		outRgb[0] = 0.15f; outRgb[1] = 0.90f; outRgb[2] = 0.25f; break; /* green */
	case DEFERRED_ELIGIBLE_APPROXIMATE:
		outRgb[0] = 0.95f; outRgb[1] = 0.85f; outRgb[2] = 0.15f; break; /* yellow */
	case DEFERRED_FORWARD_FALLBACK:
		outRgb[0] = 0.20f; outRgb[1] = 0.45f; outRgb[2] = 0.95f; break; /* blue */
	case DEFERRED_UNSUPPORTED:
		outRgb[0] = 0.95f; outRgb[1] = 0.15f; outRgb[2] = 0.85f; break; /* magenta */
	case DEFERRED_DEBUG_FORCED:
		outRgb[0] = 0.95f; outRgb[1] = 0.10f; outRgb[2] = 0.10f; break; /* red */
	default:
		outRgb[0] = 0.3f; outRgb[1] = 0.3f; outRgb[2] = 0.3f; break;
	}
}

static qboolean DH_StageIsLightmap( const shaderStage_t *stage )
{
	int b;
	if ( !stage || !stage->active ) {
		return qfalse;
	}
	for ( b = 0; b < NUM_TEXTURE_BUNDLES; b++ ) {
		if ( stage->bundle[b].tcGen == TCGEN_LIGHTMAP ||
			stage->bundle[b].lightmap == LIGHTMAP_INDEX_SHADER ||
			stage->bundle[b].lightmap == LIGHTMAP_INDEX_OFFSET ) {
			return qtrue;
		}
	}
	return qfalse;
}

static qboolean DH_StageHasComplexTcMod( const shaderStage_t *stage )
{
	int b, i;
	if ( !stage ) {
		return qfalse;
	}
	for ( b = 0; b < NUM_TEXTURE_BUNDLES; b++ ) {
		if ( stage->bundle[b].texMods ) {
			for ( i = 0; i < stage->bundle[b].numTexMods && i < TR_MAX_TEXMODS; i++ ) {
				texMod_t type = stage->bundle[b].texMods[i].type;
				if ( type != TMOD_NONE && type != TMOD_TRANSFORM && type != TMOD_SCALE ) {
					return qtrue;
				}
			}
		}
		if ( stage->bundle[b].tcGen == TCGEN_ENVIRONMENT_MAPPED ||
			stage->bundle[b].tcGen == TCGEN_ENVIRONMENT_MAPPED_FP ||
			stage->bundle[b].tcGen == TCGEN_FOG ||
			stage->bundle[b].isScreenMap ) {
			return qtrue;
		}
	}
	return qfalse;
}

static qboolean DH_StageHasAnimation( const shaderStage_t *stage )
{
	int b;
	if ( !stage ) {
		return qfalse;
	}
	for ( b = 0; b < NUM_TEXTURE_BUNDLES; b++ ) {
		if ( stage->bundle[b].numImageAnimations > 1 || stage->bundle[b].isVideoMap ) {
			return qtrue;
		}
	}
	return qfalse;
}

static qboolean DH_IsBlendedOpaque( const shader_t *shader )
{
	unsigned stageBits, srcBlend, dstBlend;
	if ( !shader || !shader->stages[0] ) {
		return qfalse;
	}
	stageBits = shader->stages[0]->stateBits;
	srcBlend = stageBits & GLS_SRCBLEND_BITS;
	dstBlend = stageBits & GLS_DSTBLEND_BITS;
	if ( srcBlend || dstBlend ) {
		/* Alpha-test opaque is OK; true blends are not. */
		if ( ( stageBits & GLS_ATEST_BITS ) != 0 &&
			srcBlend == 0 && dstBlend == 0 ) {
			return qfalse;
		}
		if ( srcBlend != 0 || dstBlend != 0 ) {
			return qtrue;
		}
	}
	if ( shader->sort >= SS_BLEND0 && shader->sort <= SS_BLEND6 ) {
		return qtrue;
	}
	return qfalse;
}

ClassicShaderMaterialInfo R_TranslateClassicShaderToMaterial( const shader_t *shader )
{
	ClassicShaderMaterialInfo info;
	int i, activeStages = 0, diffuseStages = 0, lmStages = 0;
	int firstDiffuse = -1, firstLm = -1;

	Com_Memset( &info, 0, sizeof( info ) );
	info.baseColorStage = -1;
	info.lightmapStage = -1;
	info.failReason = DEFERRED_REASON_NO_BASE_COLOR_EXPORT;

	if ( !shader ) {
		Q_strncpyz( info.summary, "null shader", sizeof( info.summary ) );
		return info;
	}

	info.stageCount = shader->numUnfoggedPasses;
	info.twoSided = ( shader->cullType == CT_TWO_SIDED ) ? qtrue : qfalse;

	for ( i = 0; i < MAX_SHADER_STAGES && i < shader->numUnfoggedPasses; i++ ) {
		const shaderStage_t *st = shader->stages[i];
		unsigned src, dst;
		if ( !st || !st->active ) {
			continue;
		}
		activeStages++;
		src = st->stateBits & GLS_SRCBLEND_BITS;
		dst = st->stateBits & GLS_DSTBLEND_BITS;

		if ( DH_StageIsLightmap( st ) ) {
			lmStages++;
			if ( firstLm < 0 ) {
				firstLm = i;
			}
			continue;
		}

		/* Diffuse-like stage: opaque or alpha-test, not pure add. */
		if ( src == GLS_SRCBLEND_ONE && dst == GLS_DSTBLEND_ONE ) {
			info.unsupportedStageCount++;
			continue;
		}
		if ( DH_StageHasComplexTcMod( st ) ) {
			info.failReason = DEFERRED_REASON_UNSUPPORTED_TCMOD;
			info.unsupportedStageCount++;
			Q_strncpyz( info.summary, "unsupported tcMod/env", sizeof( info.summary ) );
			return info;
		}
		if ( DH_StageHasAnimation( st ) ) {
			info.failReason = DEFERRED_REASON_UNSUPPORTED_ANIMATION;
			info.unsupportedStageCount++;
			Q_strncpyz( info.summary, "animated/video stage", sizeof( info.summary ) );
			return info;
		}

		diffuseStages++;
		if ( firstDiffuse < 0 ) {
			firstDiffuse = i;
		}
		if ( st->stateBits & GLS_ATEST_BITS ) {
			info.alphaTested = qtrue;
		}
#ifdef USE_VK_PBR
		if ( st->normalMap ) {
			info.hasNormalMap = qtrue;
		}
		if ( st->physicalMap || ( st->vk_pbr_flags & ( PBR_HAS_PHYSICALMAP | PBR_HAS_SPECULARMAP ) ) ) {
			info.hasSpecularOrPhysical = qtrue;
		}
		if ( st->emissiveMap || ( st->vk_pbr_flags & PBR_HAS_EMISSIVE ) ) {
			info.hasEmissive = qtrue;
		}
#endif
	}

	if ( firstDiffuse < 0 ) {
		info.failReason = DEFERRED_REASON_NO_BASE_COLOR_EXPORT;
		Q_strncpyz( info.summary, "no diffuse stage", sizeof( info.summary ) );
		return info;
	}

	/* Certified subset: 1 diffuse (+ optional lightmap). Extra non-LM stages → Forward+. */
	if ( diffuseStages > 1 ) {
		info.failReason = DEFERRED_REASON_MULTISTAGE_CLASSIC_SHADER;
		info.unsupportedStageCount = diffuseStages - 1;
		Com_sprintf( info.summary, sizeof( info.summary ),
			"multistage classic (%d diffuse)", diffuseStages );
		return info;
	}

	info.hasBaseColor = qtrue;
	info.baseColorStage = firstDiffuse;
	info.hasLightmap = ( lmStages > 0 || ( shader->lightmapIndex >= 0 ) ) ? qtrue : qfalse;
	info.lightmapStage = firstLm;
	info.valid = qtrue;
	info.failReason = DEFERRED_REASON_CLASSIC_TRANSLATED;
	Com_sprintf( info.summary, sizeof( info.summary ),
		"translated diffuse@%d lm=%s nrm=%d spec=%d emit=%d atest=%d",
		firstDiffuse, info.hasLightmap ? "yes" : "no",
		info.hasNormalMap ? 1 : 0, info.hasSpecularOrPhysical ? 1 : 0,
		info.hasEmissive ? 1 : 0, info.alphaTested ? 1 : 0 );
	return info;
}

static DeferredEligibilityResult DH_Make(
	deferredEligibility_t elig,
	deferredEligibilityReason_t reason )
{
	DeferredEligibilityResult r;
	Com_Memset( &r, 0, sizeof( r ) );
	r.eligibility = elig;
	r.reason = reason;
	r.reasonName = R_DeferredEligibilityReason_Name( reason );
	return r;
}

DeferredEligibilityResult R_GetDeferredEligibility(
	const shader_t *shader,
	const surfaceType_t *surface,
	unsigned drawSurfSortFlags,
	int viewClass )
{
	DeferredEligibilityResult res;
	const int mode = r_renderMode ? r_renderMode->integer : 0;
	int force;

	(void)surface;
	Com_Memset( &res, 0, sizeof( res ) );

	force = ri.Cvar_VariableIntegerValue( "r_deferredForceEligibility" );
	if ( force > 0 ) {
		res = DH_Make( DEFERRED_DEBUG_FORCED, DEFERRED_REASON_NONE );
		res.gbufferFlags = GBUFFER_APPROXIMATED | GBUFFER_USING_LIT_SCENE_AS_BASE;
		res.reasonName = "DEBUG_FORCED";
		return res;
	}

	if ( viewClass == VK_VIEW_CLASS_UI || viewClass == VK_VIEW_CLASS_NO_WORLD ||
		viewClass == VK_VIEW_CLASS_WEAPON ||
		( drawSurfSortFlags & ( R_PATH_FLAG_FORCE_WEAPON | R_PATH_FLAG_WEAPON_CANDIDATE ) ) ) {
		return DH_Make( DEFERRED_UNSUPPORTED, DEFERRED_REASON_WEAPON_OR_UI );
	}

	if ( shader && shader->isSky ) {
		return DH_Make( DEFERRED_UNSUPPORTED, DEFERRED_REASON_SKY );
	}

	if ( shader && shader->sort == SS_PORTAL ) {
		return DH_Make( DEFERRED_UNSUPPORTED, DEFERRED_REASON_PORTAL_OR_MIRROR );
	}

	if ( mode == 0 || R_ClassicLightingActive() ) {
		return DH_Make( DEFERRED_FORWARD_FALLBACK, DEFERRED_REASON_CLASSIC_OR_MODE0 );
	}

	if ( !vk_deferred_lighting_path_ready() || !vk_deferred_opaque_transparent_split() ) {
		return DH_Make( DEFERRED_FORWARD_FALLBACK, DEFERRED_REASON_PATH_NOT_READY );
	}

	if ( !shader ) {
		return DH_Make( DEFERRED_FORWARD_FALLBACK, DEFERRED_REASON_NO_BASE_COLOR_EXPORT );
	}

	/* Transparency / blend → never deferred opaque owner. */
	{
		unsigned stageBits = shader->stages[0] ? shader->stages[0]->stateBits : 0;
		unsigned src = stageBits & GLS_SRCBLEND_BITS;
		unsigned dst = stageBits & GLS_DSTBLEND_BITS;
		qboolean additive = ( src == GLS_SRCBLEND_ONE && dst == GLS_DSTBLEND_ONE ) ? qtrue : qfalse;
		if ( ( shader->sort >= SS_BLEND0 && shader->sort <= SS_BLEND6 ) ||
			( src == GLS_SRCBLEND_SRC_ALPHA && dst == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA ) ||
			additive ) {
			return DH_Make( DEFERRED_UNSUPPORTED, DEFERRED_REASON_TRANSPARENT );
		}
	}

	if ( DH_IsBlendedOpaque( shader ) ) {
		return DH_Make( DEFERRED_FORWARD_FALLBACK, DEFERRED_REASON_BLENDED_SURFACE );
	}

	if ( shader->numDeforms > 0 ) {
		return DH_Make( DEFERRED_FORWARD_FALLBACK, DEFERRED_REASON_DEFORMED_SURFACE );
	}

	if ( vk_transparency_is_refractive( shader ) || shader->hasScreenMap ) {
		return DH_Make( DEFERRED_UNSUPPORTED, DEFERRED_REASON_TRANSMISSION_OR_REFRACTION );
	}

#ifdef USE_VK_PBR
	if ( shader->hasPBR ) {
		unsigned flags = 0u;
		int s;
		qboolean hasN = qfalse, hasM = qfalse;
		for ( s = 0; s < MAX_SHADER_STAGES && s < shader->numUnfoggedPasses; s++ ) {
			const shaderStage_t *st = shader->stages[s];
			if ( !st || !st->active ) {
				continue;
			}
			if ( st->normalMap || ( st->vk_pbr_flags & PBR_HAS_NORMALMAP ) ) {
				hasN = qtrue;
			}
			if ( st->physicalMap || ( st->vk_pbr_flags & ( PBR_HAS_PHYSICALMAP | PBR_HAS_SPECULARMAP ) ) ) {
				hasM = qtrue;
			}
			if ( st->vk_pbr_flags & ( PBR_HAS_TRANSMISSION | PBR_HAS_ANISOTROPY | PBR_HAS_SUBSURFACE ) ) {
				return DH_Make( DEFERRED_FORWARD_FALLBACK, DEFERRED_REASON_FORWARD_ONLY_POLICY );
			}
		}
		flags |= GBUFFER_PBR_NATIVE;
		if ( hasN ) {
			flags |= GBUFFER_VALID_NORMAL;
		}
		if ( hasM ) {
			flags |= GBUFFER_VALID_MATERIAL;
		}
		flags |= GBUFFER_VALID_BASE_COLOR;
		/* Arch 0 only: SceneBaseLit is sampled as deferred "albedo". */
		if ( !R_DeferredMixedMaterialWanted() ) {
			flags |= GBUFFER_USING_LIT_SCENE_AS_BASE;
		}
		res = DH_Make( DEFERRED_ELIGIBLE_FULL, DEFERRED_REASON_PBR_NATIVE );
		res.gbufferFlags = flags;
		return res;
	}
#endif

	/* Classic translation path */
	res.classic = R_TranslateClassicShaderToMaterial( shader );
	if ( !res.classic.valid ) {
		deferredEligibilityReason_t fail = res.classic.failReason;
		/* Strict: surface unsupported for deferred — do not silently treat as normal Forward+. */
		if ( R_DeferredStrictValidationWanted() ) {
			res = DH_Make( DEFERRED_UNSUPPORTED, fail );
		} else {
			res = DH_Make( DEFERRED_FORWARD_FALLBACK, fail );
		}
		res.classic = R_TranslateClassicShaderToMaterial( shader ); /* restore details */
		res.reasonName = R_DeferredEligibilityReason_Name( res.reason );
		return res;
	}

	/*
	 * MIXED_MATERIAL_DEFERRED needs the gbuf/PBR fragment path for unlit + LM MRT packing.
	 * Pipeline export is gated on vk_pbr_flags — pure classic lightmap draws stay Forward+
	 * (hybrid arch 0 can still use SceneBaseLit additive for them).
	 */
	if ( R_DeferredMixedMaterialWanted() ) {
		qboolean canExport = qfalse;
		int s;
		for ( s = 0; s < MAX_SHADER_STAGES && s < shader->numUnfoggedPasses; s++ ) {
			const shaderStage_t *st = shader->stages[s];
			if ( st && st->active && st->vk_pbr_flags ) {
				canExport = qtrue;
				break;
			}
		}
		if ( !canExport ) {
			return DH_Make( DEFERRED_FORWARD_FALLBACK, DEFERRED_REASON_NO_BASE_COLOR_EXPORT );
		}
	}

	res.eligibility = DEFERRED_ELIGIBLE_APPROXIMATE;
	res.reason = DEFERRED_REASON_CLASSIC_TRANSLATED;
	res.reasonName = R_DeferredEligibilityReason_Name( res.reason );
	res.gbufferFlags = GBUFFER_TRANSLATED_CLASSIC | GBUFFER_APPROXIMATED | GBUFFER_VALID_BASE_COLOR;
	/* Geometry normals + default rough/metal are enough for mixed unlit export. */
	res.gbufferFlags |= GBUFFER_VALID_NORMAL | GBUFFER_VALID_MATERIAL;
	if ( res.classic.hasNormalMap ) {
		res.gbufferFlags |= GBUFFER_VALID_NORMAL;
	}
	if ( res.classic.hasSpecularOrPhysical ) {
		res.gbufferFlags |= GBUFFER_VALID_MATERIAL;
	}
	if ( !R_DeferredMixedMaterialWanted() ) {
		res.gbufferFlags |= GBUFFER_USING_LIT_SCENE_AS_BASE;
	}
	return res;
}

qboolean R_DeferredHonesty_WantsDeferredPath( const DeferredEligibilityResult *res )
{
	if ( !res ) {
		return qfalse;
	}
	if ( res->eligibility == DEFERRED_ELIGIBLE_FULL ||
		res->eligibility == DEFERRED_ELIGIBLE_APPROXIMATE ||
		res->eligibility == DEFERRED_DEBUG_FORCED ) {
		return qtrue;
	}
	return qfalse;
}

void vk_deferred_honesty_begin_frame( void )
{
	Com_Memset( &s_frame, 0, sizeof( s_frame ) );
}

void R_DeferredHonesty_NoteOpaque( void )
{
	s_frame.opaqueSurfaces++;
}

void R_DeferredHonesty_NoteEligibility( const DeferredEligibilityResult *res )
{
	if ( !res ) {
		return;
	}
	if ( res->reason >= 0 && res->reason < DEFERRED_REASON_COUNT ) {
		s_frame.reasonCounts[res->reason]++;
	}
	switch ( res->eligibility ) {
	case DEFERRED_ELIGIBLE_FULL:
		s_frame.deferredEligibleFull++;
		if ( res->gbufferFlags & GBUFFER_PBR_NATIVE ) {
			s_frame.pbrNative++;
		}
		break;
	case DEFERRED_ELIGIBLE_APPROXIMATE:
		s_frame.deferredEligibleApprox++;
		if ( res->gbufferFlags & GBUFFER_TRANSLATED_CLASSIC ) {
			s_frame.classicTranslated++;
		}
		break;
	case DEFERRED_FORWARD_FALLBACK:
		s_frame.forwardFallback++;
		break;
	case DEFERRED_UNSUPPORTED:
		s_frame.unsupported++;
		break;
	case DEFERRED_DEBUG_FORCED:
		s_frame.deferredEligibleApprox++;
		break;
	default:
		break;
	}
	if ( res->gbufferFlags & GBUFFER_VALID_NORMAL ) {
		s_frame.validNormals++;
	}
	if ( res->gbufferFlags & GBUFFER_VALID_MATERIAL ) {
		s_frame.validMaterial++;
	}
	if ( res->gbufferFlags & GBUFFER_USING_LIT_SCENE_AS_BASE ) {
		s_frame.litSceneAsBase++;
	}
}

void R_DeferredHonesty_NoteDeferredExported( unsigned gbufferFlags )
{
	s_frame.deferredExported++;
	if ( !( gbufferFlags & GBUFFER_VALID_BASE_COLOR ) ||
		!( gbufferFlags & GBUFFER_VALID_NORMAL ) ) {
		s_frame.defaultGBuffer++;
	}
}

void R_DeferredHonesty_NoteDefaultGBuffer( void )
{
	s_frame.defaultGBuffer++;
}

void R_DeferredHonesty_NoteLitSceneAsBase( void )
{
	s_frame.litSceneAsBase++;
}

void R_DeferredStatus_f( void )
{
	deferredArchitecture_t arch = (deferredArchitecture_t)(
		r_deferredArchitecture ? r_deferredArchitecture->integer : 0 );
	deferredCompositeMode_t comp = (deferredCompositeMode_t)(
		r_deferredCompositeMode ? r_deferredCompositeMode->integer : 0 );
	uint32_t eligible = s_frame.deferredEligibleFull + s_frame.deferredEligibleApprox;
	const char *brdfParity;
	const char *layout;
	const char *ownership;

	if ( arch >= DEFERRED_ARCH_MIXED_MATERIAL ) {
		brdfParity = "partial (shared GGX; mixed unlit base + deferred lightmap; sun=CSM modulate)";
		layout = ( r_gbufferCompact && r_gbufferCompact->integer )
			? "scaffold_fp16 (mixed packs LM; compact dual-write overridden on owned)"
			: "scaffold_fp16 (albedo=unlit base for owned; LM in G-buffer)";
		ownership =
			"  ownership: eligible → unlit GBufferBaseColor + deferred static(LM)+dynamic;\n"
			"             ineligible → Forward+ SceneBaseLit; composite replaces owned pixels\n";
	} else {
		brdfParity = "partial (shared GGX core; hybrid SceneBaseLit; sun=CSM modulate)";
		layout = ( r_gbufferCompact && r_gbufferCompact->integer )
			? "scaffold_fp16 + compact_dual_write"
			: "scaffold_fp16 (albedo=SceneBaseLit copy)";
		ownership =
			"  ownership: Forward+/legacy writes SceneBaseLit (lightmaps/static);\n"
			"             deferred compute adds clustered dynamics\n";
	}

	ri.Printf( PRINT_ALL, "======== deferred_status (Deferred Honesty) ========\n" );
	ri.Printf( PRINT_ALL, "activeRendererMode=r_renderMode %d\n",
		r_renderMode ? r_renderMode->integer : -1 );
	ri.Printf( PRINT_ALL, "deferredArchitecture=%d (%s)\n",
		(int)arch, R_DeferredArchitecture_Name( arch ) );
	ri.Printf( PRINT_ALL, "%s", ownership );
	ri.Printf( PRINT_ALL, "compositeMode=%d (%s)\n", (int)comp, R_DeferredCompositeMode_Name( comp ) );
	ri.Printf( PRINT_ALL, "gbufferLayout=%s\n", layout );
	ri.Printf( PRINT_ALL, "sharedBrdfParity=%s\n", brdfParity );
	ri.Printf( PRINT_ALL, "pathReady=%s lightingActive=%s directExport=%s\n",
		vk_deferred_lighting_path_ready() ? "yes" : "no",
		vk_deferred_lighting_active() ? "yes" : "no",
		vk.deferredGbufferDirectExport ? "yes" : "no" );
	ri.Printf( PRINT_ALL,
		"opaqueSurfaces=%u\n"
		"deferredEligible=%u (full=%u approx=%u)\n"
		"deferredExported=%u\n"
		"forwardFallback=%u unsupported=%u\n"
		"defaultGBufferValues=%u\n"
		"litSceneColorAsDeferredBase=%u\n"
		"validNormals=%u validMaterialParams=%u\n"
		"pbrNative=%u classicTranslated=%u\n",
		s_frame.opaqueSurfaces, eligible,
		s_frame.deferredEligibleFull, s_frame.deferredEligibleApprox,
		s_frame.deferredExported, s_frame.forwardFallback, s_frame.unsupported,
		s_frame.defaultGBuffer, s_frame.litSceneAsBase,
		s_frame.validNormals, s_frame.validMaterial,
		s_frame.pbrNative, s_frame.classicTranslated );
	ri.Printf( PRINT_ALL,
		"NOTE: Label=%s. Full BRDF parity / sun BRDF / IBL-in-compute are later phases.\n"
		"  Classic OA without translation → Forward+ (or UNSUPPORTED in strict). See docs/DEFERRED_HONESTY.md\n",
		R_DeferredArchitecture_Name( arch ) );
}

void R_MaterialTranslateStatus_f( void )
{
	const char *name;
	shader_t *sh;
	ClassicShaderMaterialInfo info;
	DeferredEligibilityResult elig;

	if ( ri.Cmd_Argc() < 2 ) {
		ri.Printf( PRINT_ALL, "usage: material_translate_status <shader-name>\n" );
		return;
	}
	name = ri.Cmd_Argv( 1 );
	sh = R_FindShader( name, LIGHTMAP_NONE, qfalse );
	if ( !sh || sh->defaultShader ) {
		ri.Printf( PRINT_ALL, "material_translate_status: shader '%s' not found\n", name );
		return;
	}
	info = R_TranslateClassicShaderToMaterial( sh );
	elig = R_GetDeferredEligibility( sh, NULL, 0, (int)VK_VIEW_CLASS_MAIN_WORLD );
	ri.Printf( PRINT_ALL, "material_translate_status: %s\n", sh->name );
	ri.Printf( PRINT_ALL, "  stages=%d unfogged=%d hasPBR=%d lightmapIndex=%d deforms=%d\n",
		info.stageCount, sh->numUnfoggedPasses,
#ifdef USE_VK_PBR
		sh->hasPBR ? 1 : 0,
#else
		0,
#endif
		sh->lightmapIndex, sh->numDeforms );
	ri.Printf( PRINT_ALL, "  translation.valid=%d summary=%s\n", info.valid ? 1 : 0, info.summary );
	ri.Printf( PRINT_ALL, "  baseColorStage=%d lightmapStage=%d unsupported=%d\n",
		info.baseColorStage, info.lightmapStage, info.unsupportedStageCount );
	ri.Printf( PRINT_ALL, "  normal=%d specular/physical=%d emissive=%d alphaTest=%d twoSided=%d\n",
		info.hasNormalMap ? 1 : 0, info.hasSpecularOrPhysical ? 1 : 0,
		info.hasEmissive ? 1 : 0, info.alphaTested ? 1 : 0, info.twoSided ? 1 : 0 );
	ri.Printf( PRINT_ALL, "  eligibility=%s reason=%s gbufferFlags=0x%x\n",
		R_DeferredEligibility_Name( elig.eligibility ), elig.reasonName, elig.gbufferFlags );
}

void vk_deferred_honesty_register( void )
{
	static qboolean s_logged;

	if ( s_registered ) {
		return;
	}

	r_deferredArchitecture = ri.Cvar_Get( "r_deferredArchitecture", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_deferredArchitecture, "0", "3", CV_INTEGER );
	ri.Cvar_SetDescription( r_deferredArchitecture,
		"Deferred Honesty architecture (latched):\n"
		" 0 = HYBRID_ADDITIVE_DEFERRED — SceneBaseLit + deferred dynamics (reference)\n"
		" 1 = MIXED_MATERIAL_DEFERRED — eligible unlit G-buffer; deferred owns LM+dynamics\n"
		" 2 = STRICT_DEFERRED_VALIDATION — mixed path; invalid surfaces shown explicitly\n"
		" 3 = DEFERRED_COMPARISON — Forward+ vs mixed deferred\n"
		"See docs/DEFERRED_HONESTY.md." );
	ri.Cvar_SetGroup( r_deferredArchitecture, CVG_RENDERER );

	r_deferredCompositeMode = ri.Cvar_Get( "r_deferredCompositeMode", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_deferredCompositeMode, "0", "3", CV_INTEGER );
	ri.Cvar_SetDescription( r_deferredCompositeMode,
		"Deferred composite:\n"
		" 0 = additive hybrid (SceneBaseLit + DeferredDynamic)\n"
		" 1 = full replace for eligible pixels (migration)\n"
		" 2 = debug side-by-side\n"
		" 3 = material-data validation" );
	ri.Cvar_SetGroup( r_deferredCompositeMode, CVG_RENDERER );

	r_deferredEligibilityDebug = ri.Cvar_Get( "r_deferredEligibilityDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_deferredEligibilityDebug, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_deferredEligibilityDebug,
		"Tint by deferred eligibility: full=green approx=yellow Forward+=blue unsupported=magenta forced=red." );
	ri.Cvar_SetGroup( r_deferredEligibilityDebug, CVG_RENDERER );

	r_gbufferInvalidPolicy = ri.Cvar_Get( "r_gbufferInvalidPolicy", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_gbufferInvalidPolicy, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_gbufferInvalidPolicy,
		"Invalid G-buffer policy: 0=debug magenta 1=Forward+ fallback (default) 2=safe unlit diagnostic." );
	ri.Cvar_SetGroup( r_gbufferInvalidPolicy, CVG_RENDERER );

	ri.Cvar_Get( "r_deferredForceEligibility", "0", CVAR_CHEAT );

	ri.Cmd_AddCommand( "deferred_status", R_DeferredStatus_f );
	ri.Cmd_AddCommand( "material_translate_status", R_MaterialTranslateStatus_f );

	s_registered = qtrue;
	if ( !s_logged ) {
		ri.Printf( PRINT_ALL,
			"[VK][deferred] architecture=%s composite=%s (honesty milestone; not full deferred)\n",
			R_DeferredArchitecture_Name( (deferredArchitecture_t)r_deferredArchitecture->integer ),
			R_DeferredCompositeMode_Name( (deferredCompositeMode_t)r_deferredCompositeMode->integer ) );
		s_logged = qtrue;
	}
}
