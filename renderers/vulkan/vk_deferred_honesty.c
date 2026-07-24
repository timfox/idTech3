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
#include "vk_renderer_iq_p1.h"
#include "vk_hdr_resolve_contract.h"
#include "tr_render_mode_vk.h"
#include "vk.h"

cvar_t *r_deferredArchitecture;
cvar_t *r_deferredCompositeMode;
cvar_t *r_deferredEligibilityDebug;
cvar_t *r_gbufferInvalidPolicy;
cvar_t *r_legacyDeferredRoughness;
cvar_t *r_legacyDeferredSpecular;
cvar_t *r_deferredLightmapMode;
cvar_t *r_deferredLightmapDebug;
cvar_t *r_deferredOwnershipDebug;
cvar_t *r_deferredCompositeDebug;
cvar_t *r_deferredArchitectureCompare;

typedef struct {
	uint32_t opaqueSurfaces;
	uint32_t deferredEligibleFull;
	uint32_t deferredEligibleApprox;
	uint32_t deferredExported;
	uint32_t trueGBufferSurfaces;
	uint32_t additiveHybridSurfaces;
	uint32_t forwardFallback;
	uint32_t unsupported;
	uint32_t defaultGBuffer;
	uint32_t litSceneAsBase;
	uint32_t gbufferBaseColor;
	uint32_t deferredLightmap;
	uint32_t forwardLightmap;
	uint32_t doubleShaded;
	uint32_t unowned;
	uint32_t invalidGBuffer;
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
	case DEFERRED_ARCH_FORWARD_PLUS_REFERENCE: return "FORWARD_PLUS_REFERENCE";
	case DEFERRED_ARCH_ADDITIVE_HYBRID: return "HYBRID_ADDITIVE_DEFERRED";
	case DEFERRED_ARCH_FULL_FIDELITY: return "FULL_FIDELITY_MATERIAL_DEFERRED";
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
	return ( r_deferredArchitecture->integer == DEFERRED_ARCH_FULL_FIDELITY ||
		r_deferredArchitecture->integer == DEFERRED_ARCH_COMPARE ||
		r_deferredArchitecture->integer == DEFERRED_ARCH_STRICT_VALIDATION ) ? qtrue : qfalse;
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
	case DEFERRED_REASON_BASE_COLOR_EXPORT_UNREPRESENTABLE: return "BASE_COLOR_EXPORT_UNREPRESENTABLE";
	default: return "UNKNOWN";
	}
}

const char *R_DeferredPixelOwner_Name( deferredPixelOwner_t owner )
{
	switch ( owner ) {
	case OPAQUE_OWNER_INVALID: return "INVALID";
	case OPAQUE_OWNER_DEFERRED: return "DEFERRED";
	case OPAQUE_OWNER_FORWARD_PLUS: return "FORWARD_PLUS";
	case OPAQUE_OWNER_LIGHTMAP_ONLY: return "LIGHTMAP_ONLY";
	case OPAQUE_OWNER_EXPLICIT_FULLBRIGHT: return "EXPLICIT_FULLBRIGHT";
	case OPAQUE_OWNER_SPECIALIZED: return "SPECIALIZED";
	default: return "UNKNOWN";
	}
}

static const char *DH_RgbGenName( int gen )
{
	switch ( gen ) {
	case CGEN_IDENTITY: return "identity";
	case CGEN_IDENTITY_LIGHTING: return "identityLighting";
	case CGEN_EXACT_VERTEX: return "exactVertex";
	case CGEN_VERTEX: return "vertex";
	case CGEN_CONST: return "const";
	case CGEN_ENTITY: return "entity";
	case CGEN_ONE_MINUS_ENTITY: return "oneMinusEntity";
	case CGEN_ONE_MINUS_VERTEX: return "oneMinusVertex";
	case CGEN_WAVEFORM: return "waveform";
	case CGEN_LIGHTING_DIFFUSE: return "lightingDiffuse";
	case CGEN_FOG: return "fog";
	default: return "bad";
	}
}

static const char *DH_TcGenName( int gen )
{
	switch ( gen ) {
	case TCGEN_TEXTURE: return "texture";
	case TCGEN_LIGHTMAP: return "lightmap";
	case TCGEN_IDENTITY: return "identity";
	case TCGEN_ENVIRONMENT_MAPPED: return "environment";
	case TCGEN_ENVIRONMENT_MAPPED_FP: return "environmentFP";
	case TCGEN_FOG: return "fog";
	case TCGEN_VECTOR: return "vector";
	default: return "bad";
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
	float roughDefault, specDefault;

	Com_Memset( &info, 0, sizeof( info ) );
	info.baseColorStage = -1;
	info.lightmapStage = -1;
	info.failReason = DEFERRED_REASON_NO_BASE_COLOR_EXPORT;
	info.rgbGen = CGEN_BAD;
	info.tcGen = TCGEN_BAD;
	roughDefault = ( r_legacyDeferredRoughness ) ? r_legacyDeferredRoughness->value : 0.72f;
	specDefault = ( r_legacyDeferredSpecular ) ? r_legacyDeferredSpecular->value : 0.04f;
	info.legacyRoughness = roughDefault;
	info.legacySpecularF0 = specDefault;
	Q_strncpyz( info.rgbGenName, "none", sizeof( info.rgbGenName ) );
	Q_strncpyz( info.tcGenName, "none", sizeof( info.tcGenName ) );

	if ( !shader ) {
		Q_strncpyz( info.summary, "null shader", sizeof( info.summary ) );
		info.translateAudit = BASE_COLOR_EXPORT_UNREPRESENTABLE;
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

		if ( src == GLS_SRCBLEND_ONE && dst == GLS_DSTBLEND_ONE ) {
			info.unsupportedStageCount++;
			continue;
		}
		if ( DH_StageHasComplexTcMod( st ) ) {
			info.failReason = DEFERRED_REASON_UNSUPPORTED_TCMOD;
			info.unsupportedStageCount++;
			info.translateAudit = BASE_COLOR_EXPORT_UNREPRESENTABLE;
			Q_strncpyz( info.summary, "unsupported tcMod/env", sizeof( info.summary ) );
			return info;
		}
		if ( DH_StageHasAnimation( st ) ) {
			info.failReason = DEFERRED_REASON_UNSUPPORTED_ANIMATION;
			info.unsupportedStageCount++;
			info.translateAudit = BASE_COLOR_EXPORT_UNREPRESENTABLE;
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

	(void)activeStages;

	if ( firstDiffuse < 0 ) {
		info.failReason = DEFERRED_REASON_NO_BASE_COLOR_EXPORT;
		info.translateAudit = BASE_COLOR_EXPORT_UNREPRESENTABLE;
		Q_strncpyz( info.summary, "no diffuse stage", sizeof( info.summary ) );
		return info;
	}

	if ( diffuseStages > 1 ) {
		info.failReason = DEFERRED_REASON_MULTISTAGE_CLASSIC_SHADER;
		info.unsupportedStageCount = diffuseStages - 1;
		info.translateAudit = BASE_COLOR_EXPORT_UNREPRESENTABLE;
		Com_sprintf( info.summary, sizeof( info.summary ),
			"multistage classic (%d diffuse)", diffuseStages );
		return info;
	}

	/* Audit base-color rgbGen / tcGen — do not guess unsupported generators. */
	{
		const shaderStage_t *base = shader->stages[firstDiffuse];
		colorGen_t rgb = base->bundle[0].rgbGen;
		texCoordGen_t tc = base->bundle[0].tcGen;

		info.rgbGen = (int)rgb;
		info.tcGen = (int)tc;
		Q_strncpyz( info.rgbGenName, DH_RgbGenName( (int)rgb ), sizeof( info.rgbGenName ) );
		Q_strncpyz( info.tcGenName, DH_TcGenName( (int)tc ), sizeof( info.tcGenName ) );

		if ( tc != TCGEN_TEXTURE && tc != TCGEN_IDENTITY && tc != TCGEN_BAD ) {
			info.failReason = DEFERRED_REASON_BASE_COLOR_EXPORT_UNREPRESENTABLE;
			info.translateAudit = BASE_COLOR_EXPORT_UNREPRESENTABLE;
			Com_sprintf( info.summary, sizeof( info.summary ),
				"unrepresentable tcGen=%s", info.tcGenName );
			return info;
		}

		switch ( rgb ) {
		case CGEN_IDENTITY:
		case CGEN_IDENTITY_LIGHTING:
			info.translateAudit |= BASE_COLOR_STAGE_VALID;
			break;
		case CGEN_VERTEX:
		case CGEN_EXACT_VERTEX:
			info.vertexColorModulation = qtrue;
			info.translateAudit |= BASE_COLOR_STAGE_VALID | BASE_COLOR_VERTEX_MODULATION_VALID;
			break;
		case CGEN_CONST:
			info.constantColorModulation = qtrue;
			info.translateAudit |= BASE_COLOR_STAGE_VALID | BASE_COLOR_CONSTANT_MODULATION_VALID;
			break;
		case CGEN_ENTITY:
			/* Entity modulate is allowed as constant-like modulation for world props. */
			info.constantColorModulation = qtrue;
			info.translateAudit |= BASE_COLOR_STAGE_VALID | BASE_COLOR_CONSTANT_MODULATION_VALID;
			break;
		default:
			info.failReason = DEFERRED_REASON_BASE_COLOR_EXPORT_UNREPRESENTABLE;
			info.translateAudit = BASE_COLOR_EXPORT_UNREPRESENTABLE;
			Com_sprintf( info.summary, sizeof( info.summary ),
				"unrepresentable rgbGen=%s", info.rgbGenName );
			return info;
		}
	}

	info.hasBaseColor = qtrue;
	info.baseColorStage = firstDiffuse;
	info.hasLightmap = ( lmStages > 0 || ( shader->lightmapIndex >= 0 ) ) ? qtrue : qfalse;
	info.lightmapStage = firstLm;
	if ( info.hasLightmap ) {
		info.translateAudit |= LIGHTMAP_STAGE_VALID;
	}
	info.materialResponseDefaulted = info.hasSpecularOrPhysical ? qfalse : qtrue;
	info.valid = qtrue;
	info.failReason = DEFERRED_REASON_CLASSIC_TRANSLATED;
	Com_sprintf( info.summary, sizeof( info.summary ),
		"translated diffuse@%d rgbGen=%s tcGen=%s lm=%s nrm=%d spec=%d emit=%d atest=%d approxMat=%d",
		firstDiffuse, info.rgbGenName, info.tcGenName,
		info.hasLightmap ? "yes" : "no",
		info.hasNormalMap ? 1 : 0, info.hasSpecularOrPhysical ? 1 : 0,
		info.hasEmissive ? 1 : 0, info.alphaTested ? 1 : 0,
		info.materialResponseDefaulted ? 1 : 0 );
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
	switch ( elig ) {
	case DEFERRED_ELIGIBLE_FULL:
		r.owner = PIXEL_OWNER_DEFERRED_FULL;
		break;
	case DEFERRED_ELIGIBLE_APPROXIMATE:
	case DEFERRED_DEBUG_FORCED:
		r.owner = PIXEL_OWNER_DEFERRED_APPROX;
		break;
	case DEFERRED_FORWARD_FALLBACK:
		r.owner = PIXEL_OWNER_FORWARD_FALLBACK;
		break;
	case DEFERRED_UNSUPPORTED:
		r.owner = ( reason == DEFERRED_REASON_SKY ) ? PIXEL_OWNER_SKY : PIXEL_OWNER_SPECIAL;
		break;
	default:
		r.owner = PIXEL_OWNER_NONE;
		break;
	}
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
			/* No explicit full-fidelity emissive MRT yet: never partially defer it. */
			if ( st->emissiveMap || ( st->vk_pbr_flags & PBR_HAS_EMISSIVE ) ) {
				return DH_Make( DEFERRED_FORWARD_FALLBACK, DEFERRED_REASON_FORWARD_ONLY_POLICY );
			}
			/* Sheen has no G-buffer channel — always Forward+ until extension buffer. */
			if ( st->vk_pbr_flags & PBR_HAS_SHEEN ) {
				return DH_Make( DEFERRED_FORWARD_FALLBACK, DEFERRED_REASON_FORWARD_ONLY_POLICY );
			}
			/* Clearcoat packs on expanded material.a; compact drops the channel. */
			if ( ( st->vk_pbr_flags & PBR_HAS_CLEARCOAT ) &&
				( r_gbufferCompact && r_gbufferCompact->integer ) ) {
				return DH_Make( DEFERRED_FORWARD_FALLBACK, DEFERRED_REASON_FORWARD_ONLY_POLICY );
			}
		}
		flags |= GBUFFER_PBR_NATIVE;
		flags |= GBUFFER_VALID_BASE_COLOR | GBUFFER_VALID_NORMAL | GBUFFER_VALID_OWNERSHIP;
		if ( hasN ) {
			/* authored normal map */
		} else {
			flags |= GBUFFER_APPROXIMATED; /* geometric normal only */
		}
		if ( hasM ) {
			flags |= GBUFFER_VALID_MATERIAL;
		} else {
			flags |= GBUFFER_VALID_MATERIAL | GBUFFER_APPROXIMATED;
		}
		if ( shader->lightmapIndex >= 0 ) {
			flags |= GBUFFER_VALID_LIGHTMAP;
		}
		if ( !R_DeferredMixedMaterialWanted() ) {
			flags |= GBUFFER_USING_LIT_SCENE_AS_BASE;
		}
		res = DH_Make( DEFERRED_ELIGIBLE_FULL, DEFERRED_REASON_PBR_NATIVE );
		res.gbufferFlags = flags;
		res.owner = PIXEL_OWNER_DEFERRED_FULL;
		return res;
	}
#endif

	/* Classic translation path */
	res.classic = R_TranslateClassicShaderToMaterial( shader );
	if ( !res.classic.valid ) {
		deferredEligibilityReason_t fail = res.classic.failReason;
		if ( R_DeferredStrictValidationWanted() ) {
			res = DH_Make( DEFERRED_UNSUPPORTED, fail );
		} else {
			res = DH_Make( DEFERRED_FORWARD_FALLBACK, fail );
		}
		res.classic = R_TranslateClassicShaderToMaterial( shader );
		res.reasonName = R_DeferredEligibilityReason_Name( res.reason );
		return res;
	}
	if ( res.classic.hasEmissive ) {
		return DH_Make( DEFERRED_FORWARD_FALLBACK, DEFERRED_REASON_FORWARD_ONLY_POLICY );
	}

	/*
	 * MIXED_MATERIAL_DEFERRED needs the gbuf/PBR fragment path for unlit + LM packing.
	 * Pure classic lightmap draws without vk_pbr_flags stay Forward+.
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
	res.owner = PIXEL_OWNER_DEFERRED_APPROX;
	res.gbufferFlags = GBUFFER_TRANSLATED_CLASSIC | GBUFFER_APPROXIMATED |
		GBUFFER_VALID_BASE_COLOR | GBUFFER_VALID_NORMAL | GBUFFER_VALID_MATERIAL |
		GBUFFER_VALID_OWNERSHIP;
	if ( res.classic.hasLightmap ) {
		res.gbufferFlags |= GBUFFER_VALID_LIGHTMAP;
	}
	if ( res.classic.hasEmissive ) {
		res.gbufferFlags |= GBUFFER_VALID_EMISSIVE;
	}
	if ( res.classic.materialResponseDefaulted ) {
		res.gbufferFlags |= GBUFFER_APPROXIMATED;
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
		s_frame.trueGBufferSurfaces++;
		if ( res->gbufferFlags & GBUFFER_PBR_NATIVE ) {
			s_frame.pbrNative++;
		}
		break;
	case DEFERRED_ELIGIBLE_APPROXIMATE:
		s_frame.deferredEligibleApprox++;
		s_frame.trueGBufferSurfaces++;
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
		s_frame.additiveHybridSurfaces++;
	}
	if ( ( res->gbufferFlags & GBUFFER_VALID_BASE_COLOR ) &&
		!( res->gbufferFlags & GBUFFER_USING_LIT_SCENE_AS_BASE ) &&
		R_DeferredHonesty_WantsDeferredPath( res ) ) {
		s_frame.gbufferBaseColor++;
	}
	if ( ( res->gbufferFlags & GBUFFER_VALID_LIGHTMAP ) &&
		R_DeferredHonesty_WantsDeferredPath( res ) &&
		R_DeferredMixedMaterialWanted() ) {
		s_frame.deferredLightmap++;
	} else if ( res->classic.hasLightmap || ( res->gbufferFlags & GBUFFER_VALID_LIGHTMAP ) ) {
		if ( res->eligibility == DEFERRED_FORWARD_FALLBACK ||
			( res->gbufferFlags & GBUFFER_USING_LIT_SCENE_AS_BASE ) ) {
			s_frame.forwardLightmap++;
		}
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
	s_frame.additiveHybridSurfaces++;
}

void R_DeferredHonesty_NoteGBufferBaseColor( void )
{
	s_frame.gbufferBaseColor++;
}

void R_DeferredHonesty_NoteDeferredLightmap( void )
{
	s_frame.deferredLightmap++;
}

void R_DeferredHonesty_NoteForwardLightmap( void )
{
	s_frame.forwardLightmap++;
}

void R_DeferredHonesty_NoteDoubleShaded( void )
{
	s_frame.doubleShaded++;
}

void R_DeferredHonesty_NoteUnowned( void )
{
	s_frame.unowned++;
}

void R_DeferredHonesty_NoteInvalidGBuffer( void )
{
	s_frame.invalidGBuffer++;
}

void R_DeferredHonesty_GetOwnershipSnapshot( deferredOwnershipSnapshot_t *out )
{
	if ( !out ) {
		return;
	}
	Com_Memset( out, 0, sizeof( *out ) );
	out->eligibleMaterials = s_frame.deferredEligibleFull + s_frame.deferredEligibleApprox;
	out->deferredOwnedDraws = s_frame.deferredExported;
	out->forwardOwnedDraws = s_frame.forwardFallback;
	out->unsupportedMaterials = s_frame.unsupported;
	out->invalidOwnerPixels = s_frame.unowned + s_frame.invalidGBuffer;
	out->doubleOwnerPixels = s_frame.doubleShaded;
	/* Explicit fullbright is an owner; accidental raw-albedo escape is never legal. */
	out->fullbrightEscapeCount = 0u;
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

	if ( R_DeferredMixedMaterialWanted() ) {
		brdfParity = "partial (shared GGX; mixed unlit base + deferred lightmap; sun=CSM modulate)";
		layout = ( r_gbufferCompact && r_gbufferCompact->integer )
			? "scaffold_fp16 + SurfaceData (compact oct in material.ba; LM+owner in surface)"
			: "scaffold_fp16 + SurfaceData (albedo=unlit base; LM+owner in surface MRT)";
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
		"trueGBufferSurfaces=%u additiveHybridSurfaces=%u\n"
		"deferredExported=%u\n"
		"forwardFallback=%u unsupported=%u\n"
		"pixelsUsingSceneBaseLit=%u\n"
		"pixelsUsingGBufferBaseColor=%u\n"
		"pixelsDeferredLightmap=%u pixelsForwardLightmap=%u\n"
		"doubleShadedPixels=%u unownedPixels=%u invalidGBufferPixels=%u\n"
		"defaultGBufferValues=%u\n"
		"validNormals=%u validMaterialParams=%u\n"
		"pbrNative=%u classicTranslated=%u\n",
		s_frame.opaqueSurfaces, eligible,
		s_frame.deferredEligibleFull, s_frame.deferredEligibleApprox,
		s_frame.trueGBufferSurfaces, s_frame.additiveHybridSurfaces,
		s_frame.deferredExported, s_frame.forwardFallback, s_frame.unsupported,
		s_frame.litSceneAsBase, s_frame.gbufferBaseColor,
		s_frame.deferredLightmap, s_frame.forwardLightmap,
		s_frame.doubleShaded, s_frame.unowned, s_frame.invalidGBuffer,
		s_frame.defaultGBuffer,
		s_frame.validNormals, s_frame.validMaterial,
		s_frame.pbrNative, s_frame.classicTranslated );
	ri.Printf( PRINT_ALL,
		"NOTE: Label=%s. Arch0=Forward+ reference; Arch1=legacy hybrid; Arch2=full-fidelity.\n"
		"  doubleShaded/unowned should be 0 in MIXED_MATERIAL_DEFERRED.\n"
		"  Full sun BRDF + sky IBL + SurfaceData LM/owner shipping (M3); local probes remain.\n"
		"  Compact clearcoat still Forward+; expanded mixed clearcoat uses material.a.\n"
		"  See docs/DEFERRED_HONESTY.md\n",
		R_DeferredArchitecture_Name( arch ) );
}

static void R_DeferredArchitectureValidate_f( void )
{
	const int arch = r_deferredArchitecture ? r_deferredArchitecture->integer : -1;
	const int quality = vk_gbuffer_quality_effective();
	qboolean ok = qtrue;

	if ( arch < DEFERRED_ARCH_FORWARD_PLUS_REFERENCE ||
		arch > DEFERRED_ARCH_STRICT_VALIDATION ) {
		ok = qfalse;
	}
	if ( ( arch == DEFERRED_ARCH_FULL_FIDELITY ||
		   arch == DEFERRED_ARCH_STRICT_VALIDATION ) && quality != 2 ) {
		ok = qfalse;
	}
	if ( arch == DEFERRED_ARCH_STRICT_VALIDATION &&
		( s_frame.unowned != 0u || s_frame.doubleShaded != 0u ||
		  s_frame.invalidGBuffer != 0u ) ) {
		ok = qfalse;
	}
	ri.Printf( ok ? PRINT_ALL : PRINT_ERROR,
		"deferred_architecture_validate: %s arch=%d(%s) quality=%d "
		"invalidOwner=%u doubleOwner=%u SceneHDRGeneration=%u\n",
		ok ? "PASS" : "FAIL", arch,
		R_DeferredArchitecture_Name( (deferredArchitecture_t)arch ), quality,
		s_frame.unowned + s_frame.invalidGBuffer, s_frame.doubleShaded,
		vk_hdr_resolve_scene_hdr_generation() );
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
	ri.Printf( PRINT_ALL, "  translation.valid=%d audit=0x%x summary=%s\n",
		info.valid ? 1 : 0, info.translateAudit, info.summary );
	ri.Printf( PRINT_ALL, "  baseColorStage=%d rgbGen=%s (%d) vertexMod=%d constMod=%d\n",
		info.baseColorStage, info.rgbGenName, info.rgbGen,
		info.vertexColorModulation ? 1 : 0, info.constantColorModulation ? 1 : 0 );
	ri.Printf( PRINT_ALL, "  tcGen=%s lightmapStage=%d lightmapValid=%d unsupported=%d\n",
		info.tcGenName, info.lightmapStage, ( info.translateAudit & LIGHTMAP_STAGE_VALID ) ? 1 : 0,
		info.unsupportedStageCount );
	ri.Printf( PRINT_ALL, "  logicalBaseColor=diffuse×rgbGen (lightmap NOT multiplied)\n" );
	ri.Printf( PRINT_ALL, "  materialResponse: rough=%g F0=%g defaulted=%d (approx flag)\n",
		info.legacyRoughness, info.legacySpecularF0, info.materialResponseDefaulted ? 1 : 0 );
	ri.Printf( PRINT_ALL, "  normal=%d specular/physical=%d emissive=%d alphaTest=%d twoSided=%d\n",
		info.hasNormalMap ? 1 : 0, info.hasSpecularOrPhysical ? 1 : 0,
		info.hasEmissive ? 1 : 0, info.alphaTested ? 1 : 0, info.twoSided ? 1 : 0 );
	ri.Printf( PRINT_ALL, "  eligibility=%s owner=%s reason=%s gbufferFlags=0x%x\n",
		R_DeferredEligibility_Name( elig.eligibility ),
		R_DeferredPixelOwner_Name( elig.owner ),
		elig.reasonName, elig.gbufferFlags );
}

void vk_deferred_honesty_register( void )
{
	static qboolean s_logged;

	if ( s_registered ) {
		return;
	}

	r_deferredArchitecture = ri.Cvar_Get( "r_deferredArchitecture", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_deferredArchitecture, "0", "4", CV_INTEGER );
	ri.Cvar_SetDescription( r_deferredArchitecture,
		"Deferred Honesty architecture (latched):\n"
		" 0 = FORWARD_PLUS_REFERENCE\n"
		" 1 = LEGACY_HYBRID_ADDITIVE_DEFERRED\n"
		" 2 = FULL_FIDELITY_MATERIAL_DEFERRED (production target)\n"
		" 3 = DEFERRED_FORWARD_COMPARISON\n"
		" 4 = STRICT_OWNERSHIP_VALIDATION\n"
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

	r_legacyDeferredRoughness = ri.Cvar_Get( "r_legacyDeferredRoughness", "0.72", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_legacyDeferredRoughness, "0.04", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_legacyDeferredRoughness,
		"Default perceptual roughness for certified classic materials without physical maps." );
	ri.Cvar_SetGroup( r_legacyDeferredRoughness, CVG_RENDERER );

	r_legacyDeferredSpecular = ri.Cvar_Get( "r_legacyDeferredSpecular", "0.04", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_legacyDeferredSpecular, "0", "0.08", CV_FLOAT );
	ri.Cvar_SetDescription( r_legacyDeferredSpecular,
		"Default dielectric F0 for certified classic diffuse materials (migration tuning)." );
	ri.Cvar_SetGroup( r_legacyDeferredSpecular, CVG_RENDERER );

	r_deferredLightmapMode = ri.Cvar_Get( "r_deferredLightmapMode", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_deferredLightmapMode, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_deferredLightmapMode,
		"Deferred lightmap: 0=non-directional irradiance 1=deluxe directional diffuse (when packed) 2=debug compare." );
	ri.Cvar_SetGroup( r_deferredLightmapMode, CVG_RENDERER );

	r_deferredLightmapDebug = ri.Cvar_Get( "r_deferredLightmapDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_deferredLightmapDebug, "0", "5", CV_INTEGER );
	ri.Cvar_SetDescription( r_deferredLightmapDebug,
		"1=raw LM 2=decoded LM 3=deluxe dir 4=static diffuse 5=LM validity." );
	ri.Cvar_SetGroup( r_deferredLightmapDebug, CVG_RENDERER );

	r_deferredOwnershipDebug = ri.Cvar_Get( "r_deferredOwnershipDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_deferredOwnershipDebug, "0", "3", CV_INTEGER );
	ri.Cvar_SetDescription( r_deferredOwnershipDebug,
		"1=owner colors 2=double writes (red) 3=unowned (magenta)." );
	ri.Cvar_SetGroup( r_deferredOwnershipDebug, CVG_RENDERER );

	r_deferredCompositeDebug = ri.Cvar_Get( "r_deferredCompositeDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_deferredCompositeDebug, "0", "4", CV_INTEGER );
	ri.Cvar_SetDescription( r_deferredCompositeDebug,
		"1=deferred input 2=Forward+ fallback 3=ownership 4=combined." );
	ri.Cvar_SetGroup( r_deferredCompositeDebug, CVG_RENDERER );

	r_deferredArchitectureCompare = ri.Cvar_Get( "r_deferredArchitectureCompare", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_deferredArchitectureCompare, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_deferredArchitectureCompare,
		"1=split left arch0 hybrid / right arch1 mixed (requires vid_restart + arch latch care)." );
	ri.Cvar_SetGroup( r_deferredArchitectureCompare, CVG_RENDERER );

	ri.Cvar_Get( "r_deferredForceEligibility", "0", CVAR_CHEAT );
	ri.Cvar_Get( "r_materialExportCompare", "0", CVAR_CHEAT );
	ri.Cvar_Get( "r_lightmapParityCompare", "0", CVAR_CHEAT );
	ri.Cvar_Get( "r_deferredLightingParity", "0", CVAR_CHEAT ); /* 1=diff 2=sun 3=local 4=shadow 5=LM 6=final */
	ri.Cvar_Get( "r_deferredSunBrdf", "1", CVAR_ARCHIVE_ND ); /* 1=enable sun BRDF in mixed deferred */
	ri.Cvar_Get( "r_deferredIbl", "1", CVAR_ARCHIVE_ND ); /* 1=sky IBL in mixed deferred lighting */
	ri.Cvar_Get( "r_deferredIblStrength", "1", CVAR_ARCHIVE_ND );

	ri.Cmd_AddCommand( "deferred_status", R_DeferredStatus_f );
	ri.Cmd_AddCommand( "deferred_architecture_status", R_DeferredStatus_f );
	ri.Cmd_AddCommand( "deferred_architecture_validate", R_DeferredArchitectureValidate_f );
	ri.Cmd_AddCommand( "material_translate_status", R_MaterialTranslateStatus_f );

	s_registered = qtrue;
	if ( !s_logged ) {
		ri.Printf( PRINT_ALL,
			"[VK][deferred] architecture=%s composite=%s (honesty M2; mixed unlit+LM when arch>=1)\n",
			R_DeferredArchitecture_Name( (deferredArchitecture_t)r_deferredArchitecture->integer ),
			R_DeferredCompositeMode_Name( (deferredCompositeMode_t)r_deferredCompositeMode->integer ) );
		s_logged = qtrue;
	}
}
