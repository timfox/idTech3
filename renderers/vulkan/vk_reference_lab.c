/*
===========================================================================
Raster Ultra 1.11 — Rendering Reference Lab (validation only).
===========================================================================
*/

#include "tr_local.h"
#include "vk_reference_lab.h"

static cvar_t *r_referenceLab;
static cvar_t *r_referenceLabScene;
static cvar_t *r_referenceLabSeed;
static cvar_t *r_referenceLabDecomp;
static cvar_t *r_referenceLabMode;
static cvar_t *r_referenceLabSupersample;
static cvar_t *r_referenceLabFreezeAnim;
static qboolean s_cmds;
static vkRefLabState_t s_lab;

typedef struct {
	const char *name;
	const char *mapHint;
	const char *cfgHint;
	vkRefLabBookmark_t bookmarks[3];
	int bookmarkCount;
} vkRefLabSceneDesc_t;

static const vkRefLabSceneDesc_t s_scenes[VK_REFLAB_SCENE_COUNT] = {
	{ "material_spheres", "rtest_pbr", "modern_raster_reference.cfg",
		{ { "front", { 0, -256, 64 }, { 0, 90, 0 } }, { "oblique", { 128, -200, 80 }, { -10, 120, 0 } }, { "close", { 0, -96, 48 }, { 0, 90, 0 } } }, 3 },
	{ "rough_metal_sweep", "rtest_pbr", "modern_raster_reference.cfg",
		{ { "sweep", { 0, -320, 48 }, { 0, 90, 0 } }, { "end", { 256, -320, 48 }, { 0, 90, 0 } }, { "top", { 0, -200, 200 }, { -45, 90, 0 } } }, 3 },
	{ "advanced_lobes", "rtest_pbr", "vulkan_overlay_raster_ultra_1_8_materials.cfg",
		{ { "clearcoat", { 0, -200, 64 }, { 0, 90, 0 } }, { "sheen", { 64, -200, 64 }, { 0, 90, 0 } }, { "aniso", { -64, -200, 64 }, { 0, 90, 0 } } }, 3 },
	{ "hard_edges", "rtest_tangent", "modern_raster_reference.cfg",
		{ { "edge", { 0, -128, 32 }, { 0, 90, 0 } }, { "silhouette", { 96, -96, 48 }, { -5, 135, 0 } }, { "grazing", { 0, -64, 16 }, { 5, 90, 0 } } }, 3 },
	{ "tangent_parity", "rtest_tangent", "modern_raster_reference.cfg",
		{ { "flat", { 0, -160, 40 }, { 0, 90, 0 } }, { "cylinder", { 64, -160, 40 }, { 0, 90, 0 } }, { "seam", { -64, -160, 40 }, { 0, 90, 0 } } }, 3 },
	{ "direct_lights", "rtest_parity", "modern_clustered.cfg",
		{ { "key", { 0, -256, 96 }, { -15, 90, 0 } }, { "fill", { 128, -200, 64 }, { -10, 110, 0 } }, { "rim", { -128, -200, 64 }, { -10, 70, 0 } } }, 3 },
	{ "area_lights", "rtest_parity", "modern_clustered.cfg",
		{ { "panel", { 0, -220, 80 }, { -10, 90, 0 } }, { "soft", { 80, -180, 60 }, { -5, 100, 0 } }, { "wide", { 0, -300, 100 }, { -20, 90, 0 } } }, 3 },
	{ "shadows", "rtest_parity", "vulkan_overlay_raster_ultra_1_9_virtual_shadows.cfg",
		{ { "cascade", { 0, -400, 120 }, { -20, 90, 0 } }, { "contact", { 0, -80, 24 }, { 0, 90, 0 } }, { "moving", { 100, -200, 64 }, { -10, 100, 0 } } }, 3 },
	{ "gi", "rtest_parity", "modern_raster_ultra.cfg",
		{ { "bounce", { 0, -200, 64 }, { 0, 90, 0 } }, { "corner", { 80, -80, 40 }, { -5, 135, 0 } }, { "ceiling", { 0, -160, 120 }, { -30, 90, 0 } } }, 3 },
	{ "reflections", "rtest_parity", "modern_raster_ultra.cfg",
		{ { "planar", { 0, -200, 48 }, { 0, 90, 0 } }, { "probe", { 64, -180, 64 }, { -5, 100, 0 } }, { "ssr", { 0, -120, 32 }, { 10, 90, 0 } } }, 3 },
	{ "water", "rtest_volumetric", "modern_raster_ultra.cfg",
		{ { "surface", { 0, -200, 80 }, { -15, 90, 0 } }, { "edge", { 100, -100, 40 }, { 0, 120, 0 } }, { "under", { 0, 0, -32 }, { 10, 90, 0 } } }, 3 },
	{ "transparency", "demo_oit", "demo_oit_clustered.cfg",
		{ { "stack", { 0, -180, 64 }, { 0, 90, 0 } }, { "sort", { 64, -160, 48 }, { -5, 110, 0 } }, { "reveal", { -64, -160, 48 }, { -5, 70, 0 } } }, 3 },
	{ "particles", "rtest_postfx", "modern_raster_ultra.cfg",
		{ { "emit", { 0, -160, 64 }, { 0, 90, 0 } }, { "side", { 120, -120, 48 }, { -5, 120, 0 } }, { "dense", { 0, -80, 40 }, { 0, 90, 0 } } }, 3 },
	{ "decals", "rtest_parity", "modern_raster_ultra.cfg",
		{ { "wall", { 0, -100, 48 }, { 0, 90, 0 } }, { "floor", { 0, -60, 16 }, { -40, 90, 0 } }, { "overlap", { 40, -80, 32 }, { -10, 100, 0 } } }, 3 },
	{ "volumetrics", "rtest_volumetric", "vulkan_overlay_raster_ultra_1_7_atmosphere.cfg",
		{ { "fog", { 0, -256, 64 }, { 0, 90, 0 } }, { "shaft", { 64, -200, 96 }, { -20, 100, 0 } }, { "local", { 0, -120, 40 }, { 0, 90, 0 } } }, 3 },
	{ "atmosphere", "outdoor", "vulkan_overlay_raster_ultra_1_7_atmosphere.cfg",
		{ { "horizon", { 0, 0, 64 }, { -5, 0, 0 } }, { "zenith", { 0, 0, 64 }, { -80, 0, 0 } }, { "sunset", { 0, 0, 64 }, { -10, 90, 0 } } }, 3 },
	{ "weather", "outdoor", "vulkan_overlay_raster_ultra_1_7_atmosphere.cfg",
		{ { "rain", { 0, -100, 48 }, { 0, 90, 0 } }, { "snow", { 0, -100, 48 }, { 0, 90, 0 } }, { "fog", { 0, -200, 40 }, { 0, 90, 0 } } }, 3 },
	{ "terrain", "openworld", "modern_raster_ultra.cfg",
		{ { "ridge", { 0, -500, 200 }, { -20, 90, 0 } }, { "valley", { 200, -300, 80 }, { -10, 120, 0 } }, { "distant", { 0, -1000, 300 }, { -25, 90, 0 } } }, 3 },
	{ "foliage", "outdoor", "modern_raster_ultra.cfg",
		{ { "canopy", { 0, -120, 64 }, { 0, 90, 0 } }, { "trunk", { 40, -60, 32 }, { 0, 100, 0 } }, { "wind", { 0, -200, 48 }, { -5, 90, 0 } } }, 3 },
	{ "lod", "rtest_parity", "vulkan_overlay_raster_ultra_1_6_geometry.cfg",
		{ { "near", { 0, -64, 48 }, { 0, 90, 0 } }, { "mid", { 0, -256, 64 }, { 0, 90, 0 } }, { "far", { 0, -1024, 96 }, { -5, 90, 0 } } }, 3 },
	{ "streaming", "openworld", "modern_raster_ultra.cfg",
		{ { "sector0", { 0, 0, 64 }, { 0, 0, 0 } }, { "boundary", { 512, 0, 64 }, { 0, 0, 0 } }, { "teleport", { 2048, 0, 64 }, { 0, 180, 0 } } }, 3 },
	{ "hdr_presentation", "rtest_postfx", "vulkan_overlay_raster_ultra_1_10_hdr_presentation.cfg",
		{ { "bright", { 0, -160, 64 }, { 0, 90, 0 } }, { "dark", { 0, -80, 32 }, { 0, 90, 0 } }, { "doorway", { 0, -200, 48 }, { 0, 90, 0 } } }, 3 },
	{ "weapon_ui", "any", "modern_vulkan.cfg",
		{ { "hip", { 0, 0, 0 }, { 0, 0, 0 } }, { "ads", { 0, 0, 0 }, { 0, 0, 0 } }, { "menu", { 0, 0, 0 }, { 0, 0, 0 } } }, 3 },
};

void vk_reference_lab_register_cvars( void )
{
	if ( r_referenceLab ) {
		return;
	}
	r_referenceLab = ri.Cvar_Get( "r_referenceLab", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_referenceLab, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_referenceLab,
		"Raster Ultra 1.11 Reference Lab (latched).\n"
		"Deterministic validation mode — no new rendering techniques.\n"
		"Pins grain/exposure/temporal jitter for reproducible captures." );
	ri.Cvar_SetGroup( r_referenceLab, CVG_RENDERER );

	r_referenceLabScene = ri.Cvar_Get( "r_referenceLabScene", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_referenceLabScene, "0", va( "%d", VK_REFLAB_SCENE_COUNT - 1 ), CV_INTEGER );

	r_referenceLabSeed = ri.Cvar_Get( "r_referenceLabSeed", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_referenceLabSeed, "0", "2147483647", CV_INTEGER );

	r_referenceLabDecomp = ri.Cvar_Get( "r_referenceLabDecomp", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_referenceLabDecomp, "0", va( "%d", VK_REFLAB_DECOMP_COUNT - 1 ), CV_INTEGER );
	ri.Cvar_SetDescription( r_referenceLabDecomp,
		"Lighting decomposition intent: 0 final, 1 direct, 2 indirect, 3 shadows, 4 AO, 5 reflections, 6 emissive, 7 volumetrics." );

	r_referenceLabMode = ri.Cvar_Get( "r_referenceLabMode", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_referenceLabMode, "0", "6", CV_INTEGER );
	ri.Cvar_SetDescription( r_referenceLabMode,
		"Reference mode: 0 none, 1–3 spatial SS 2/4/8x, 4 material, 5 lighting, 6 presentation." );

	r_referenceLabSupersample = ri.Cvar_Get( "r_referenceLabSupersample", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_referenceLabSupersample, "1", "8", CV_INTEGER );

	r_referenceLabFreezeAnim = ri.Cvar_Get( "r_referenceLabFreezeAnim", "1", CVAR_ARCHIVE_ND );
}

void vk_reference_lab_init( void )
{
	vk_reference_lab_register_cvars();
	Com_Memset( &s_lab, 0, sizeof( s_lab ) );
	s_lab.seed = 1;
	s_lab.supersampleScale = 1;
	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "reference_lab_status", vk_reference_lab_status_f );
		ri.Cmd_AddCommand( "reference_lab_scenes", vk_reference_lab_list_scenes_f );
		s_cmds = qtrue;
	}
	ri.Printf( PRINT_ALL,
		"[VK][ReferenceLab] %s scenes=%d (deterministic validation; no new techniques)\n",
		( r_referenceLab && r_referenceLab->integer ) ? "enabled" : "off",
		VK_REFLAB_SCENE_COUNT );
}

void vk_reference_lab_shutdown( void )
{
	Com_Memset( &s_lab, 0, sizeof( s_lab ) );
}

qboolean vk_reference_lab_active( void )
{
	return ( r_referenceLab && r_referenceLab->integer ) ? qtrue : qfalse;
}

const vkRefLabState_t *vk_reference_lab_state( void )
{
	return &s_lab;
}

const char *vk_reference_lab_scene_name( vkRefLabScene_t scene )
{
	if ( scene < 0 || scene >= VK_REFLAB_SCENE_COUNT ) {
		return "invalid";
	}
	return s_scenes[scene].name;
}

const char *vk_reference_lab_decomp_name( vkRefLabDecomp_t d )
{
	static const char *names[] = {
		"final", "direct", "indirect", "shadows", "ao", "reflections", "emissive", "volumetrics"
	};
	if ( d < 0 || d >= VK_REFLAB_DECOMP_COUNT ) {
		return "invalid";
	}
	return names[d];
}

int vk_reference_lab_bookmark_count( vkRefLabScene_t scene )
{
	if ( scene < 0 || scene >= VK_REFLAB_SCENE_COUNT ) {
		return 0;
	}
	return s_scenes[scene].bookmarkCount;
}

const vkRefLabBookmark_t *vk_reference_lab_bookmark( vkRefLabScene_t scene, int index )
{
	if ( scene < 0 || scene >= VK_REFLAB_SCENE_COUNT ) {
		return NULL;
	}
	if ( index < 0 || index >= s_scenes[scene].bookmarkCount ) {
		return NULL;
	}
	return &s_scenes[scene].bookmarks[index];
}

void vk_reference_lab_begin_frame( void )
{
	int scene;
	int mode;
	int ss;
	const vkRefLabSceneDesc_t *desc;

	if ( !vk_reference_lab_active() ) {
		return;
	}

	scene = r_referenceLabScene ? r_referenceLabScene->integer : 0;
	scene = (int)Com_Clamp( 0, VK_REFLAB_SCENE_COUNT - 1, (float)scene );
	mode = r_referenceLabMode ? r_referenceLabMode->integer : 0;
	ss = r_referenceLabSupersample ? r_referenceLabSupersample->integer : 1;
	if ( ss != 1 && ss != 2 && ss != 4 && ss != 8 ) {
		ss = 1;
	}

	desc = &s_scenes[scene];
	s_lab.scene = (vkRefLabScene_t)scene;
	s_lab.decomp = (vkRefLabDecomp_t)(int)Com_Clamp( 0, VK_REFLAB_DECOMP_COUNT - 1,
		(float)( r_referenceLabDecomp ? r_referenceLabDecomp->integer : 0 ) );
	s_lab.referenceMode = (vkRefLabReferenceMode_t)(int)Com_Clamp( 0, 6, (float)mode );
	s_lab.seed = (uint32_t)( r_referenceLabSeed ? r_referenceLabSeed->integer : 1 );
	s_lab.frameCount++;
	s_lab.deterministic = qtrue;
	s_lab.freezeAnimation = ( !r_referenceLabFreezeAnim || r_referenceLabFreezeAnim->integer ) ? qtrue : qfalse;
	s_lab.freezeWeather = qtrue;
	s_lab.freezeExposure = qtrue;
	s_lab.freezeTemporalJitter = qtrue;
	s_lab.noGrain = qtrue;
	s_lab.noLens = qtrue;
	s_lab.mapHint = desc->mapHint;
	s_lab.cfgHint = desc->cfgHint;
	s_lab.supersampleScale = ss;
	s_lab.supersampleActive = ( ss > 1 ) ? qtrue : qfalse;

	/* Material / presentation reference pins */
	if ( s_lab.referenceMode == VK_REFLAB_REF_MATERIAL ||
		s_lab.referenceMode == VK_REFLAB_REF_PRESENTATION ||
		s_lab.referenceMode == VK_REFLAB_REF_LIGHTING ) {
		s_lab.noBloom = qtrue;
		ri.Cvar_Set( "r_bloom", "0" );
		ri.Cvar_Set( "r_filmGrain", "0" );
		ri.Cvar_Set( "r_chromaticAberration", "0" );
		ri.Cvar_Set( "r_exposure_auto", "0" );
		ri.Cvar_Set( "r_exposureFixed", "1" );
		ri.Cvar_Set( "r_captureDeterministic", "1" );
	}
	if ( s_lab.referenceMode >= VK_REFLAB_REF_SPATIAL_2X &&
		s_lab.referenceMode <= VK_REFLAB_REF_SPATIAL_8X ) {
		/* Spatial SS reference: no temporal history */
		ri.Cvar_Set( "r_taa", "0" );
		ri.Cvar_Set( "r_aaMode", "0" );
		s_lab.freezeTemporalJitter = qtrue;
		if ( s_lab.referenceMode == VK_REFLAB_REF_SPATIAL_2X ) {
			s_lab.supersampleScale = 2;
		} else if ( s_lab.referenceMode == VK_REFLAB_REF_SPATIAL_4X ) {
			s_lab.supersampleScale = 4;
		} else {
			s_lab.supersampleScale = 8;
		}
		s_lab.supersampleActive = qtrue;
	}

	/* Always pin grain/CA for lab determinism */
	ri.Cvar_Set( "r_filmGrain", "0" );
	ri.Cvar_Set( "r_chromaticAberration", "0" );
	if ( s_lab.freezeExposure ) {
		ri.Cvar_Set( "r_captureDeterministic", "1" );
	}
}

void vk_reference_lab_list_scenes_f( void )
{
	int i;

	ri.Printf( PRINT_ALL, "=== Reference Lab scenes (%d) ===\n", VK_REFLAB_SCENE_COUNT );
	for ( i = 0; i < VK_REFLAB_SCENE_COUNT; i++ ) {
		ri.Printf( PRINT_ALL, " %2d %-22s map=%-16s cfg=%s bookmarks=%d\n",
			i, s_scenes[i].name, s_scenes[i].mapHint, s_scenes[i].cfgHint,
			s_scenes[i].bookmarkCount );
	}
}

void vk_reference_lab_status_f( void )
{
	const vkRefLabBookmark_t *bm;

	ri.Printf( PRINT_ALL, "=== Reference Lab (Raster Ultra 1.11) ===\n" );
	ri.Printf( PRINT_ALL, "active         : %s deterministic=%s\n",
		vk_reference_lab_active() ? "yes" : "no",
		s_lab.deterministic ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "scene          : %d %s\n",
		(int)s_lab.scene, vk_reference_lab_scene_name( s_lab.scene ) );
	ri.Printf( PRINT_ALL, "decomp         : %s\n", vk_reference_lab_decomp_name( s_lab.decomp ) );
	ri.Printf( PRINT_ALL, "refMode        : %d supersample=%dx\n",
		(int)s_lab.referenceMode, s_lab.supersampleScale );
	ri.Printf( PRINT_ALL, "seed/frames    : %u / %u\n", s_lab.seed, s_lab.frameCount );
	ri.Printf( PRINT_ALL, "pins           : anim=%s weather=%s exposure=%s jitter=%s bloom_off=%s\n",
		s_lab.freezeAnimation ? "yes" : "no",
		s_lab.freezeWeather ? "yes" : "no",
		s_lab.freezeExposure ? "yes" : "no",
		s_lab.freezeTemporalJitter ? "yes" : "no",
		s_lab.noBloom ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "hints          : map=%s cfg=%s\n",
		s_lab.mapHint ? s_lab.mapHint : "-", s_lab.cfgHint ? s_lab.cfgHint : "-" );
	bm = vk_reference_lab_bookmark( s_lab.scene, 0 );
	if ( bm ) {
		ri.Printf( PRINT_ALL, "bookmark0      : %s origin=(%.0f,%.0f,%.0f) angles=(%.0f,%.0f,%.0f)\n",
			bm->name, bm->origin[0], bm->origin[1], bm->origin[2],
			bm->angles[0], bm->angles[1], bm->angles[2] );
	}
	ri.Printf( PRINT_ALL, "policy         : no new techniques; evidence-based promotion; boot unchanged\n" );
}
