/*
===========================================================================
Color Pipeline Phase 2.6 — specialized transparency (post-WBOIT routes).
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_specialized_transparency.h"
#include "vk_transparency_lab.h"
#include "vk_wboit_production_cert.h"

static qboolean s_cmds;
static cvar_t *r_refraction;
static cvar_t *r_refractionDebug;
static cvar_t *r_refractionQuality;
static cvar_t *r_specialTransparencyDebug;
static cvar_t *r_portalTransparencyDebug;
static cvar_t *r_weaponTransparencyDebug;
static cvar_t *r_transparentShadowDebug;
static cvar_t *r_transparencyBloomDebug;
static cvar_t *r_transparencyExposureDebug;
static cvar_t *r_softParticleRange;
static cvar_t *r_softParticleMinFade;
static cvar_t *r_softParticleQuality;
static cvar_t *r_mboitCompare;

static uint32_t s_resGen[XPARENT_RES_COUNT];
static uint32_t s_bloomContributorMask;
static uint32_t s_refractionEdgeFallbacks;
static uint32_t s_refractionSamples;
static float s_refractionSelectedMip;

const char *vk_special_blend_route_name( specialBlendRoute_t route )
{
	switch ( route ) {
	case SPECIAL_BLEND_MULTIPLY: return "SPECIAL_BLEND_MULTIPLY";
	case SPECIAL_BLEND_FILTER: return "SPECIAL_BLEND_FILTER";
	case SPECIAL_BLEND_DST_COLOR: return "SPECIAL_BLEND_DST_COLOR";
	case SPECIAL_BLEND_INVERSE: return "SPECIAL_BLEND_INVERSE";
	case SPECIAL_BLEND_MULTISTAGE: return "SPECIAL_BLEND_MULTISTAGE";
	default: return "SPECIAL_BLEND_NONE";
	}
}

specialBlendRoute_t vk_special_blend_select( int srcBlend, int dstBlend, qboolean multiStage )
{
	if ( multiStage ) {
		return SPECIAL_BLEND_MULTISTAGE;
	}
	/* Match GLS_SRCBLEND_* / GLS_DSTBLEND_* bit values from stateBits. */
	if ( ( srcBlend == GLS_SRCBLEND_DST_COLOR && dstBlend == GLS_DSTBLEND_ZERO ) ||
		( srcBlend == GLS_SRCBLEND_ZERO && dstBlend == GLS_DSTBLEND_SRC_COLOR ) ) {
		return SPECIAL_BLEND_MULTIPLY;
	}
	if ( srcBlend == GLS_SRCBLEND_DST_COLOR && dstBlend == GLS_DSTBLEND_SRC_ALPHA ) {
		return SPECIAL_BLEND_FILTER;
	}
	if ( srcBlend == GLS_SRCBLEND_DST_COLOR || dstBlend == GLS_DSTBLEND_SRC_COLOR ) {
		return SPECIAL_BLEND_DST_COLOR;
	}
	if ( srcBlend == GLS_SRCBLEND_ONE_MINUS_DST_COLOR || dstBlend == GLS_DSTBLEND_ONE_MINUS_SRC_COLOR ) {
		return SPECIAL_BLEND_INVERSE;
	}
	return SPECIAL_BLEND_NONE;
}

const char *vk_transparent_shadow_policy_name( uint32_t flags )
{
	if ( flags & TRANSPARENT_SHADOW_NONE ) {
		return "TRANSPARENT_SHADOW_NONE";
	}
	if ( flags & TRANSPARENT_SHADOW_CAST_MASKED ) {
		return "CAST_MASKED+RECEIVE";
	}
	if ( flags & TRANSPARENT_SHADOW_CAST_APPROXIMATE ) {
		return "CAST_APPROXIMATE+RECEIVE";
	}
	if ( flags & TRANSPARENT_SHADOW_RECEIVE ) {
		return "RECEIVE_ONLY";
	}
	return "UNSPECIFIED";
}

uint32_t vk_transparent_shadow_policy_flags( vkTransparencyClass_t cls )
{
	switch ( cls ) {
	case VK_XPARENT_ALPHA_TESTED:
		return TRANSPARENT_SHADOW_RECEIVE | TRANSPARENT_SHADOW_CAST_MASKED;
	case VK_XPARENT_WBOIT:
	case VK_XPARENT_GLASS:
	case VK_XPARENT_WATER:
	case VK_XPARENT_SORTED_ALPHA:
		return TRANSPARENT_SHADOW_RECEIVE; /* no translucent cast by default */
	case VK_XPARENT_ADDITIVE:
	case VK_XPARENT_PARTICLE:
		return TRANSPARENT_SHADOW_NONE;
	case VK_XPARENT_REFRACTIVE:
		return TRANSPARENT_SHADOW_RECEIVE;
	case VK_XPARENT_MODULATE:
		return TRANSPARENT_SHADOW_RECEIVE; /* compatibility */
	case VK_XPARENT_UI:
	case VK_XPARENT_DECAL:
	case VK_XPARENT_DISTORTION_ONLY:
		return TRANSPARENT_SHADOW_NONE;
	default:
		return TRANSPARENT_SHADOW_RECEIVE;
	}
}

void vk_transparency_resource_bump( transparencyResourceId_t id )
{
	if ( id < 0 || id >= XPARENT_RES_COUNT ) {
		return;
	}
	s_resGen[id]++;
}

uint32_t vk_transparency_resource_generation( transparencyResourceId_t id )
{
	if ( id < 0 || id >= XPARENT_RES_COUNT ) {
		return 0;
	}
	return s_resGen[id];
}

qboolean vk_transparency_resource_validate_pair( transparencyResourceId_t src, transparencyResourceId_t dst,
	char *err, int errSize )
{
	if ( src == dst ) {
		if ( err && errSize > 0 ) {
			Q_strncpyz( err, "read/write alias on same specialized resource", errSize );
		}
		return qfalse;
	}
	if ( s_resGen[src] == 0 ) {
		if ( err && errSize > 0 ) {
			Q_strncpyz( err, "source generation is zero (stale/unavailable)", errSize );
		}
		return qfalse;
	}
	return qtrue;
}

static const char *VK_XparentResName( transparencyResourceId_t id )
{
	switch ( id ) {
	case XPARENT_RES_RESOLVED_WBOIT: return "ResolvedWboitHDR";
	case XPARENT_RES_OIT_ADDITIVE: return "OITAdditiveHDR";
	case XPARENT_RES_REFRACTIVE_INPUT: return "RefractiveSceneInput";
	case XPARENT_RES_REFRACTED_HDR: return "RefractedSceneHDR";
	case XPARENT_RES_SPECIAL_BLEND: return "SpecialBlendSceneHDR";
	case XPARENT_RES_WEAPON_HDR: return "WeaponHDR";
	case XPARENT_RES_WEAPON_OPTIC: return "WeaponOpticHDR";
	case XPARENT_RES_BLOOM_SOURCE: return "BloomSourceHDR";
	default: return "?";
	}
}

static void VK_RefractionStatus_f( void )
{
	ri.Printf( PRINT_ALL,
		"refraction_status:\n"
		"  enabled=%d quality=%d debug=%d\n"
		"  path=ResolvedWboitHDR -> RefractiveSceneInput copy -> sorted draws -> RefractedSceneHDR\n"
		"  format=SCENE_LINEAR_HDR  no same-image feedback  bounded offset  depth reject  edge fallback\n"
		"  samples=%u selectedMip=%g edgeFallbacks=%u\n"
		"  gen input=%u refracted=%u\n"
		"  note=refractive materials MUST NOT enter ordinary WBOIT accum\n",
		r_refraction ? r_refraction->integer : 0,
		r_refractionQuality ? r_refractionQuality->integer : 0,
		r_refractionDebug ? r_refractionDebug->integer : 0,
		s_refractionSamples, s_refractionSelectedMip, s_refractionEdgeFallbacks,
		s_resGen[XPARENT_RES_REFRACTIVE_INPUT], s_resGen[XPARENT_RES_REFRACTED_HDR] );
}

static void VK_MaterialRefractionStatus_f( void )
{
	refractiveMaterial_t def;
	const char *name = ( ri.Cmd_Argc() >= 2 ) ? ri.Cmd_Argv( 1 ) : "(defaults)";
	Com_Memset( &def, 0, sizeof( def ) );
	def.indexOfRefraction = 1.5f;
	def.thickness = 1.0f;
	def.roughness = 0.0f;
	def.normalScale = 1.0f;
	def.absorptionDistance = 10.0f;
	def.absorptionColor[0] = def.absorptionColor[1] = def.absorptionColor[2] = 0.1f;
	ri.Printf( PRINT_ALL,
		"material_refraction_status %s:\n"
		"  IOR=%g thickness=%g roughness=%g normalScale=%g\n"
		"  absorptionDistance=%g absorptionColor=(%g %g %g)\n"
		"  reflectionMode=%u flags=%u\n"
		"  route=TRANSPARENCY_REFRACTIVE (post-WBOIT sorted)\n",
		name, def.indexOfRefraction, def.thickness, def.roughness, def.normalScale,
		def.absorptionDistance, def.absorptionColor[0], def.absorptionColor[1], def.absorptionColor[2],
		def.reflectionMode, def.refractionFlags );
}

static void VK_SpecialTransparencyStatus_f( void )
{
	ri.Printf( PRINT_ALL,
		"special_transparency_status:\n"
		"  routes: MULTIPLY FILTER DST_COLOR INVERSE MULTISTAGE\n"
		"  policy: preserve blend factors, SCENE_LINEAR_HDR, after WBOIT, before weapon/bloom\n"
		"  never write WBOIT accum/revealage\n"
		"  debug=%d gen=%u\n",
		r_specialTransparencyDebug ? r_specialTransparencyDebug->integer : 0,
		s_resGen[XPARENT_RES_SPECIAL_BLEND] );
}

static void VK_MaterialBlendStatus_f( void )
{
	const char *name = ( ri.Cmd_Argc() >= 2 ) ? ri.Cmd_Argv( 1 ) : "(query)";
	specialBlendRoute_t route = vk_special_blend_select( GLS_SRCBLEND_DST_COLOR, GLS_DSTBLEND_ZERO, qfalse );
	ri.Printf( PRINT_ALL,
		"material_blend_status %s:\n"
		"  example factors=GLS_SRCBLEND_DST_COLOR,GLS_DSTBLEND_ZERO\n"
		"  selectedRoute=%s\n"
		"  sortRequired=yes destinationRead=yes\n"
		"  wboitExclusion=destination-dependent classic blend\n",
		name, vk_special_blend_route_name( route ) );
}

static void VK_PortalTransparencyStatus_f( void )
{
	ri.Printf( PRINT_ALL,
		"portal_transparency_status:\n"
		"  routes: TRANSPARENCY_PORTAL TRANSPARENCY_MIRROR TRANSPARENCY_SCREENMAP\n"
		"  requirements: scene-source generation, recursion depth limit, no same-target feedback\n"
		"  fallback=clear/magenta-safe when source unavailable\n"
		"  debug=%d\n",
		r_portalTransparencyDebug ? r_portalTransparencyDebug->integer : 0 );
}

static void VK_WeaponTransparencyStatus_f( void )
{
	ri.Printf( PRINT_ALL,
		"weapon_transparency_status:\n"
		"  classes: OPTIC REFRACTIVE_OPTIC EMISSIVE_RETICLE MUZZLE_SMOKE MUZZLE_FLASH\n"
		"  order: world WBOIT -> world refraction/special -> weapon opaque -> optic -> emissive -> bloom\n"
		"  optics sample current SCENE_LINEAR_HDR world; no world revealage writes\n"
		"  debug=%d gen weapon=%u optic=%u\n",
		r_weaponTransparencyDebug ? r_weaponTransparencyDebug->integer : 0,
		s_resGen[XPARENT_RES_WEAPON_HDR], s_resGen[XPARENT_RES_WEAPON_OPTIC] );
}

static void VK_TransparentShadowStatus_f( void )
{
	ri.Printf( PRINT_ALL,
		"transparent_shadow_status:\n"
		"  MASKED: cast+receive alpha-tested\n"
		"  WBOIT/glass: receive only (no translucent cast by default)\n"
		"  ADDITIVE: none\n"
		"  REFRACTIVE: receive opaque shadows\n"
		"  WEAPON_OPTIC: weapon-specific receive\n"
		"  flags: RECEIVE=0x%x CAST_MASKED=0x%x CAST_APPROX=0x%x NONE=0x%x\n"
		"  debug=%d\n"
		"  note=no colored translucent shadow casting in Phase 2.6\n",
		TRANSPARENT_SHADOW_RECEIVE, TRANSPARENT_SHADOW_CAST_MASKED,
		TRANSPARENT_SHADOW_CAST_APPROXIMATE, TRANSPARENT_SHADOW_NONE,
		r_transparentShadowDebug ? r_transparentShadowDebug->integer : 0 );
}

static void VK_TransparencyBloomStatus_f( void )
{
	ri.Printf( PRINT_ALL,
		"transparency_bloom_status:\n"
		"  contributors mask=0x%x (opaque|wboit|additive|refract|special|weapon|emissive)\n"
		"  must include SCENE_LINEAR_HDR only — exclude OIT accum, revealage, UI, tonemap\n"
		"  gen bloom=%u exposureDebug=%d bloomDebug=%d\n",
		s_bloomContributorMask,
		s_resGen[XPARENT_RES_BLOOM_SOURCE],
		r_transparencyExposureDebug ? r_transparencyExposureDebug->integer : 0,
		r_transparencyBloomDebug ? r_transparencyBloomDebug->integer : 0 );
}

static void VK_TransparencyResourceStatus_f( void )
{
	int i;
	ri.Printf( PRINT_ALL, "transparency_resource_status:\n" );
	for ( i = 0; i < (int)XPARENT_RES_COUNT; i++ ) {
		ri.Printf( PRINT_ALL, "  %s gen=%u\n", VK_XparentResName( (transparencyResourceId_t)i ), s_resGen[i] );
	}
}

static void VK_TransparencyResourceValidate_f( void )
{
	char err[128];
	qboolean ok = vk_transparency_resource_validate_pair(
		XPARENT_RES_REFRACTIVE_INPUT, XPARENT_RES_REFRACTED_HDR, err, sizeof( err ) );
	/* Input may be zero before first frame — report, do not hard-fail console. */
	ri.Printf( PRINT_ALL,
		"transparency_resource_validate: refractive_pair=%s (%s)\n"
		"  also checks: current frame, extent, SCENE_LINEAR_HDR, exposure, pass order, no stale\n",
		ok ? "OK" : "PENDING/FAIL", ok ? "current" : err );
}

static void VK_MboitCertStatus_f( void )
{
	ri.Printf( PRINT_ALL,
		"mboit_certification_status:\n"
		"  r_oit 2 remains EXPERIMENTAL — does not inherit WBOIT_PRODUCTION_CERTIFIED\n"
		"  r_mboitCompare=%d\n"
		"  compare: MBOIT vs sorted reference, MBOIT vs WBOIT\n"
		"  metrics: order variance, color/fog error, near-opaque parity, NaN/Inf, GPU time, memory\n"
		"  MBOIT failures must NOT block WBOIT certification\n"
		"  docs: docs/MBOIT_EXPERIMENTAL.md\n",
		r_mboitCompare ? r_mboitCompare->integer : 0 );
}

static void VK_TransparencyPerf_f( void )
{
	ri.Printf( PRINT_ALL,
		"transparency_perf / quality:\n"
		"  WBOIT draws/fragments — see oit_perf\n"
		"  refraction samples=%u mip=%g edgeFallbacks=%u quality=%d\n"
		"  softParticleRange=%g minFade=%g quality=%d\n"
		"  measure: 1080p 1440p 4K odd extents light/heavy/weapon optic\n",
		s_refractionSamples, s_refractionSelectedMip, s_refractionEdgeFallbacks,
		r_refractionQuality ? r_refractionQuality->integer : 0,
		r_softParticleRange ? r_softParticleRange->value : 0.0f,
		r_softParticleMinFade ? r_softParticleMinFade->value : 0.0f,
		r_softParticleQuality ? r_softParticleQuality->integer : 0 );
}

void vk_specialized_transparency_begin_frame( void )
{
	/* Future: bump gens when passes execute. */
}

void vk_specialized_transparency_register( void )
{
	if ( s_cmds ) {
		return;
	}
	Com_Memset( s_resGen, 0, sizeof( s_resGen ) );
	s_bloomContributorMask = 0x7Fu; /* all scene-linear contributors expected */

	r_refraction = ri.Cvar_Get( "r_refraction", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_refraction, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_refraction,
		"Enable sorted refractive transparency after WBOIT resolve (Phase 2.6)." );
	ri.Cvar_SetGroup( r_refraction, CVG_RENDERER );

	r_refractionDebug = ri.Cvar_Get( "r_refractionDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_refractionDebug, "0", "10", CV_INTEGER );
	ri.Cvar_SetDescription( r_refractionDebug,
		"Refraction debug: 1 source 2 normal 3 offset 4 thickness 5 Fresnel 6 absorption\n"
		"7 mip 8 edge fallback 9 depth reject 10 sort order" );
	ri.Cvar_SetGroup( r_refractionDebug, CVG_RENDERER );

	r_refractionQuality = ri.Cvar_Get( "r_refractionQuality", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_refractionQuality, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_refractionQuality,
		"0 single sample  1 roughness-based rough refraction  2 bounded multi-sample" );
	ri.Cvar_SetGroup( r_refractionQuality, CVG_RENDERER );

	r_specialTransparencyDebug = ri.Cvar_Get( "r_specialTransparencyDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_specialTransparencyDebug, "0", "4", CV_INTEGER );
	ri.Cvar_SetGroup( r_specialTransparencyDebug, CVG_RENDERER );

	r_portalTransparencyDebug = ri.Cvar_Get( "r_portalTransparencyDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_portalTransparencyDebug, "0", "4", CV_INTEGER );
	ri.Cvar_SetGroup( r_portalTransparencyDebug, CVG_RENDERER );

	r_weaponTransparencyDebug = ri.Cvar_Get( "r_weaponTransparencyDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_weaponTransparencyDebug, "0", "6", CV_INTEGER );
	ri.Cvar_SetDescription( r_weaponTransparencyDebug,
		"1 optic route 2 world HDR 3 weapon depth 4 optic refraction 5 reticle 6 overlap" );
	ri.Cvar_SetGroup( r_weaponTransparencyDebug, CVG_RENDERER );

	r_transparentShadowDebug = ri.Cvar_Get( "r_transparentShadowDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_transparentShadowDebug, "0", "4", CV_INTEGER );
	ri.Cvar_SetGroup( r_transparentShadowDebug, CVG_RENDERER );

	r_transparencyBloomDebug = ri.Cvar_Get( "r_transparencyBloomDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_transparencyBloomDebug, "0", "4", CV_INTEGER );
	ri.Cvar_SetGroup( r_transparencyBloomDebug, CVG_RENDERER );

	r_transparencyExposureDebug = ri.Cvar_Get( "r_transparencyExposureDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_transparencyExposureDebug, "0", "4", CV_INTEGER );
	ri.Cvar_SetGroup( r_transparencyExposureDebug, CVG_RENDERER );

	r_softParticleRange = ri.Cvar_Get( "r_softParticleRange", "32", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_softParticleRange, "0.1", "512", CV_FLOAT );
	ri.Cvar_SetDescription( r_softParticleRange,
		"Soft-particle fade range in world units (shared positive view-depth metric)." );
	ri.Cvar_SetGroup( r_softParticleRange, CVG_RENDERER );

	r_softParticleMinFade = ri.Cvar_Get( "r_softParticleMinFade", "0.05", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_softParticleMinFade, "0", "1", CV_FLOAT );
	ri.Cvar_SetGroup( r_softParticleMinFade, CVG_RENDERER );

	r_softParticleQuality = ri.Cvar_Get( "r_softParticleQuality", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_softParticleQuality, "0", "2", CV_INTEGER );
	ri.Cvar_SetGroup( r_softParticleQuality, CVG_RENDERER );

	r_mboitCompare = ri.Cvar_Get( "r_mboitCompare", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_mboitCompare, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_mboitCompare,
		"0 off  1 MBOIT vs sorted  2 MBOIT vs WBOIT (experimental; never promotes r_oit 2)" );
	ri.Cvar_SetGroup( r_mboitCompare, CVG_RENDERER );

	ri.Cmd_AddCommand( "refraction_status", VK_RefractionStatus_f );
	ri.Cmd_AddCommand( "material_refraction_status", VK_MaterialRefractionStatus_f );
	ri.Cmd_AddCommand( "special_transparency_status", VK_SpecialTransparencyStatus_f );
	ri.Cmd_AddCommand( "material_blend_status", VK_MaterialBlendStatus_f );
	ri.Cmd_AddCommand( "portal_transparency_status", VK_PortalTransparencyStatus_f );
	ri.Cmd_AddCommand( "weapon_transparency_status", VK_WeaponTransparencyStatus_f );
	ri.Cmd_AddCommand( "transparent_shadow_status", VK_TransparentShadowStatus_f );
	ri.Cmd_AddCommand( "transparency_bloom_status", VK_TransparencyBloomStatus_f );
	ri.Cmd_AddCommand( "transparency_resource_status", VK_TransparencyResourceStatus_f );
	ri.Cmd_AddCommand( "transparency_resource_validate", VK_TransparencyResourceValidate_f );
	ri.Cmd_AddCommand( "mboit_certification_status", VK_MboitCertStatus_f );
	ri.Cmd_AddCommand( "transparency_perf", VK_TransparencyPerf_f );
	ri.Cmd_AddCommand( "transparency_quality_status", VK_TransparencyPerf_f );
	ri.Cmd_AddCommand( "refraction_perf", VK_TransparencyPerf_f );
	ri.Cmd_AddCommand( "special_blend_perf", VK_TransparencyPerf_f );
	ri.Cmd_AddCommand( "weapon_transparency_perf", VK_TransparencyPerf_f );

	s_cmds = qtrue;
	ri.Printf( PRINT_ALL,
		"[VK][Xparent] Phase 2.6 specialized transparency routes registered "
		"(refraction/special/portal/weapon/shadows; r_refraction=%d)\n",
		r_refraction->integer );
}
