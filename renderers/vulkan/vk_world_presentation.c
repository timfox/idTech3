/*
 * Independently implemented from public behavioral specifications and
 * published graphics techniques. No proprietary third-party engine code used.
 *
 * World presentation feature registry and exposure-volume controllers.
 */
#include "tr_local.h"

#ifdef USE_VULKAN

#include "vk.h"
#include "vk_world_presentation.h"
#include "vk_exposure_volumes.h"
#include "vk_sky_environment.h"
#include "vk_environment_probes.h"
#include "vk_water_presentation.h"
#include "vk_projected_lights.h"
#include "vk_world_feature_support.h"

static worldFeatureInfo_t s_features[] = {
	{ WORLD_FEATURE_HDR_SKY, "hdr_sky", "vk_skybox_hdr / luminance",
		"sky → SceneHDR → meter → tonemap", "HDR cubemap EXR",
		"scene-linear sky radiance", "unexposed until tonemap", "reversed-Z far",
		"opaque sky", "<0.25ms meter @1440p target", WORLD_CERT_CERTIFIED, qtrue },
	{ WORLD_FEATURE_SKY_ENVIRONMENT, "sky_environment", "vk_sky_environment",
		"far sky → sky env → main opaque", "scaled secondary draw list",
		"scene-linear", "shared adaptedExposure", "depth-safe compose",
		"opaque env only", "proportional to sky scene", WORLD_CERT_SCAFFOLD, qfalse },
	{ WORLD_FEATURE_REFLECTION_PROBES, "reflection_probes", "vk_environment_probes",
		"prepass bake / shading sample", "radiance+irradiance cubemaps",
		"IBL into SceneHDR", "capture exposure fixed", "n/a",
		"opaque materials", "no per-frame capture gameplay", WORLD_CERT_SCAFFOLD, qfalse },
	{ WORLD_FEATURE_WATER, "water", "vk_water_presentation",
		"after opaque SceneHDR", "reflection/refraction sources",
		"specialized transparent", "shared exposure", "reversed-Z soft",
		"dedicated water route", "one reflection source / region", WORLD_CERT_SCAFFOLD, qfalse },
	{ WORLD_FEATURE_PROJECTED_LIGHTS, "projected_lights", "vk_projected_lights",
		"clustered lighting", "cookie + shadow depth",
		"direct light SceneHDR", "unexposed lights", "reversed-Z bias",
		"opaque receivers; alpha-test cast", "capacity bounded", WORLD_CERT_SCAFFOLD, qfalse },
	{ WORLD_FEATURE_DECALS, "decals", "vk_deferred_decals + geometry",
		"gbuffer / mesh overlay", "decal atlas",
		"modulates albedo/normal", "unexposed", "reversed-Z reconstruct",
		"not sky; not weapon", "bounded visible count", WORLD_CERT_PARTIAL, qtrue },
	{ WORLD_FEATURE_DETAIL_TEXTURES, "detail_textures", "tr_shader detail stages",
		"material shading", "detail maps",
		"bounded albedo/normal", "unexposed", "n/a",
		"opaque/compatible", "mip+fade", WORLD_CERT_PARTIAL, qtrue },
	{ WORLD_FEATURE_LIGHTSTYLES, "lightstyles", "vk_world_feature_support",
		"GPU style table", "compact float table",
		"scales lightmap/emissive", "unexposed", "n/a",
		"n/a", "table upload amortized", WORLD_CERT_SCAFFOLD, qfalse },
	{ WORLD_FEATURE_LOCAL_FOG, "local_fog", "vk_world_feature_support",
		"fog volumes → shading/WBOIT", "volume list",
		"transmittance in SceneHDR", "shared fog owner", "view-depth",
		"WBOIT integrated once", "bounded active volumes", WORLD_CERT_SCAFFOLD, qfalse },
	{ WORLD_FEATURE_COLOR_CORRECTION, "color_correction", "post LUT volumes",
		"after tonemap, before UI", "3D LUT(s)",
		"display-linear grade", "post-exposure", "n/a",
		"n/a", "≤2 blended LUTs stable", WORLD_CERT_PARTIAL, qtrue },
	{ WORLD_FEATURE_MATERIAL_DRIVERS, "material_drivers", "vk_world_feature_support",
		"CPU/GPU param buffer", "driver graph",
		"animates material params", "n/a", "n/a",
		"n/a", "fixed op budget", WORLD_CERT_SCAFFOLD, qfalse },
	{ WORLD_FEATURE_VISIBILITY_PORTALS, "visibility_portals", "vk_world_feature_support",
		"area connectivity", "portal records",
		"none", "n/a", "n/a",
		"n/a", "BSP incremental", WORLD_CERT_SCAFFOLD, qfalse },
	{ WORLD_FEATURE_DISPLACEMENTS, "terrain_patches", "vk_world_feature_support",
		"terrain draw", "height/normal grids",
		"standard opaque", "unexposed", "reversed-Z",
		"opaque", "LOD patches", WORLD_CERT_SCAFFOLD, qfalse },
	{ WORLD_FEATURE_VIEWMODEL_LIGHTING, "viewmodel_lighting", "vk_world_feature_support",
		"weapon pass", "key/ambient/probe",
		"SceneHDR weapon", "shared adaptedExposure", "near clip policy",
		"weapon; no world TAA hist", "cheap", WORLD_CERT_SCAFFOLD, qfalse },
};

static worldExposureSettings_t s_exposureSettings;
static qboolean s_cmds;

static worldFeatureInfo_t *FindFeatureByName( const char *name )
{
	int i;
	if ( !name || !name[0] ) {
		return NULL;
	}
	for ( i = 0; i < (int)( sizeof( s_features ) / sizeof( s_features[0] ) ); i++ ) {
		if ( !Q_stricmp( s_features[i].name, name ) ) {
			return &s_features[i];
		}
	}
	return NULL;
}

static worldFeatureInfo_t *FindFeatureByBit( worldPresentationFeature_t bit )
{
	int i;
	for ( i = 0; i < (int)( sizeof( s_features ) / sizeof( s_features[0] ) ); i++ ) {
		if ( s_features[i].bit == bit ) {
			return &s_features[i];
		}
	}
	return NULL;
}

static const char *CertName( worldFeatureCert_t c )
{
	switch ( c ) {
	case WORLD_CERT_CERTIFIED: return "certified";
	case WORLD_CERT_PARTIAL: return "partial";
	case WORLD_CERT_SCAFFOLD: return "scaffold";
	default: return "absent";
	}
}

void vk_world_exposure_settings_defaults( worldExposureSettings_t *out )
{
	if ( !out ) {
		return;
	}
	Com_Memset( out, 0, sizeof( *out ) );
	out->minEV = -4.0f;
	out->maxEV = 4.0f;
	out->compensation = 0.0f;
	out->brightenSpeed = 0.8f;
	out->darkenSpeed = 3.5f;
	out->lowPercentile = 0.05f;
	out->highPercentile = 0.05f;
	out->skyWeight = 0.55f;
	out->centerWeight = 0.70f;
}

void vk_world_exposure_settings_apply( const worldExposureSettings_t *settings )
{
	char buf[32];
	if ( !settings ) {
		return;
	}
	s_exposureSettings = *settings;
	/* Map volume settings onto the single shared AE path — never per-pass. */
	Com_sprintf( buf, sizeof( buf ), "%g", settings->brightenSpeed );
	ri.Cvar_Set( "r_autoExposure_speedUp", buf );
	Com_sprintf( buf, sizeof( buf ), "%g", settings->darkenSpeed );
	ri.Cvar_Set( "r_autoExposure_speedDown", buf );
	Com_sprintf( buf, sizeof( buf ), "%g", settings->lowPercentile );
	ri.Cvar_Set( "r_autoExposure_lowPercent", buf );
	Com_sprintf( buf, sizeof( buf ), "%g", settings->highPercentile );
	ri.Cvar_Set( "r_autoExposure_highPercent", buf );
	Com_sprintf( buf, sizeof( buf ), "%g", settings->skyWeight );
	ri.Cvar_Set( "r_exposureSkyWeight", buf );
	Com_sprintf( buf, sizeof( buf ), "%g", settings->centerWeight );
	ri.Cvar_Set( "r_autoExposure_centerWeight", buf );
	Com_sprintf( buf, sizeof( buf ), "%g", settings->compensation );
	ri.Cvar_Set( "r_exposureComp", buf );
	Com_sprintf( buf, sizeof( buf ), "%g", settings->minEV );
	ri.Cvar_Set( "r_exposureMinEV", buf );
	Com_sprintf( buf, sizeof( buf ), "%g", settings->maxEV );
	ri.Cvar_Set( "r_exposureMaxEV", buf );
}

const worldExposureSettings_t *vk_world_exposure_settings_current( void )
{
	return &s_exposureSettings;
}

uint32_t vk_world_presentation_enabled_mask( void )
{
	uint32_t mask = 0;
	int i;
	for ( i = 0; i < (int)( sizeof( s_features ) / sizeof( s_features[0] ) ); i++ ) {
		if ( s_features[i].enabled ) {
			mask |= (uint32_t)s_features[i].bit;
		}
	}
	return mask;
}

qboolean vk_world_presentation_feature_enabled( worldPresentationFeature_t bit )
{
	worldFeatureInfo_t *f = FindFeatureByBit( bit );
	return ( f && f->enabled ) ? qtrue : qfalse;
}

void vk_world_presentation_set_feature( worldPresentationFeature_t bit, qboolean enable )
{
	worldFeatureInfo_t *f = FindFeatureByBit( bit );
	if ( f ) {
		f->enabled = enable;
	}
}

const worldFeatureInfo_t *vk_world_presentation_feature_info( worldPresentationFeature_t bit )
{
	return FindFeatureByBit( bit );
}

static void World_PrintFeature( const worldFeatureInfo_t *f )
{
	if ( !f ) {
		return;
	}
	ri.Printf( PRINT_ALL,
		"  %-20s en=%d cert=%-9s owner=%s\n"
		"    pass=%s\n"
		"    inputs=%s SceneHDR=%s exposure=%s\n"
		"    depth=%s transparency=%s cost=%s\n",
		f->name, f->enabled ? 1 : 0, CertName( f->certification ), f->owner,
		f->passPosition, f->resourceInputs, f->sceneHdrContribution, f->exposureState,
		f->depthConvention, f->transparencyRoute, f->perfCost );
}

static void WorldFeatures_Status_f( void )
{
	int i;
	ri.Printf( PRINT_ALL, "======== world_features_status ========\n" );
	ri.Printf( PRINT_ALL, "enabled_mask=0x%08x\n", vk_world_presentation_enabled_mask() );
	for ( i = 0; i < (int)( sizeof( s_features ) / sizeof( s_features[0] ) ); i++ ) {
		World_PrintFeature( &s_features[i] );
	}
	ri.Printf( PRINT_ALL, "=======================================\n" );
}

static void WorldFeatures_Validate_f( void )
{
	int fails = 0;
	int i;
	ri.Printf( PRINT_ALL, "======== world_features_validate ========\n" );
	for ( i = 0; i < (int)( sizeof( s_features ) / sizeof( s_features[0] ) ); i++ ) {
		if ( s_features[i].enabled && s_features[i].certification == WORLD_CERT_ABSENT ) {
			ri.Printf( PRINT_WARNING, "FAIL: %s enabled but absent\n", s_features[i].name );
			fails++;
		}
	}
	if ( !ri.Cvar_VariableIntegerValue( "r_exposure_auto" ) &&
		vk_world_presentation_feature_enabled( WORLD_FEATURE_HDR_SKY ) ) {
		ri.Printf( PRINT_WARNING, "WARN: hdr_sky enabled but r_exposure_auto=0\n" );
	}
	ri.Printf( PRINT_ALL, "world_features_validate: %s (%d fail)\n", fails ? "FAIL" : "PASS", fails );
}

static void WorldFeature_Status_f( void )
{
	worldFeatureInfo_t *f;
	if ( ri.Cmd_Argc() < 2 ) {
		ri.Printf( PRINT_ALL, "usage: world_feature_status <feature>\n" );
		return;
	}
	f = FindFeatureByName( ri.Cmd_Argv( 1 ) );
	if ( !f ) {
		ri.Printf( PRINT_ALL, "unknown feature '%s'\n", ri.Cmd_Argv( 1 ) );
		return;
	}
	World_PrintFeature( f );
}

static void WorldFeature_Enable_f( void )
{
	worldFeatureInfo_t *f;
	if ( ri.Cmd_Argc() < 2 ) {
		ri.Printf( PRINT_ALL, "usage: world_feature_enable <feature>\n" );
		return;
	}
	f = FindFeatureByName( ri.Cmd_Argv( 1 ) );
	if ( !f ) {
		ri.Printf( PRINT_ALL, "unknown feature '%s'\n", ri.Cmd_Argv( 1 ) );
		return;
	}
	f->enabled = qtrue;
	ri.Printf( PRINT_ALL, "enabled %s\n", f->name );
}

static void WorldFeature_Disable_f( void )
{
	worldFeatureInfo_t *f;
	if ( ri.Cmd_Argc() < 2 ) {
		ri.Printf( PRINT_ALL, "usage: world_feature_disable <feature>\n" );
		return;
	}
	f = FindFeatureByName( ri.Cmd_Argv( 1 ) );
	if ( !f ) {
		ri.Printf( PRINT_ALL, "unknown feature '%s'\n", ri.Cmd_Argv( 1 ) );
		return;
	}
	f->enabled = qfalse;
	ri.Printf( PRINT_ALL, "disabled %s\n", f->name );
}

static void World_PerfStatus_f( void )
{
	ri.Printf( PRINT_ALL,
		"======== world_perf_status ========\n"
		"  HDR exposure meter: budget <0.25ms @1440p (target)\n"
		"  sky_environment: proportional to sky draw list\n"
		"  reflection_probes: no per-frame capture in gameplay\n"
		"  water: one shared reflection source per region\n"
		"  projected_lights: clustered + capacity bounded\n"
		"  decals: bounded visible count / tile overlap\n"
		"  material_drivers: fixed max ops\n"
		"  local_fog: bounded active volumes\n"
		"  color_correction: ≤2 blended 3D LUTs stable profile\n"
		"===================================\n" );
}

static void World_MemoryStatus_f( void )
{
	ri.Printf( PRINT_ALL,
		"======== world_memory_status ========\n"
		"  feature registry: %zu bytes static\n"
		"  exposure settings: %zu bytes\n"
		"  probe/water/light scaffolds: see module status commands\n"
		"=====================================\n",
		sizeof( s_features ), sizeof( s_exposureSettings ) );
}

void vk_world_presentation_register( void )
{
	vk_world_exposure_settings_defaults( &s_exposureSettings );

	/* Metering aliases (shared AE path). */
	ri.Cvar_Get( "r_exposureMode", "3", CVAR_ARCHIVE_ND );
	ri.Cvar_Get( "r_exposureMinEV", "-4", CVAR_ARCHIVE_ND );
	ri.Cvar_Get( "r_exposureMaxEV", "4", CVAR_ARCHIVE_ND );
	ri.Cvar_Get( "r_exposureCompensation", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_Get( "r_exposureLowPercentile", "0.05", CVAR_ARCHIVE_ND );
	ri.Cvar_Get( "r_exposureHighPercentile", "0.05", CVAR_ARCHIVE_ND );
	ri.Cvar_Get( "r_exposureBrightenSpeed", "0.8", CVAR_ARCHIVE_ND );
	ri.Cvar_Get( "r_exposureDarkenSpeed", "3.5", CVAR_ARCHIVE_ND );
	ri.Cvar_Get( "r_exposureCenterWeight", "0.70", CVAR_ARCHIVE_ND );
	/* r_exposureSkyWeight already owned by histogram module. */

	vk_exposure_volumes_register();
	vk_sky_environment_register();
	vk_environment_probes_register();
	vk_water_presentation_register();
	vk_projected_lights_register();
	vk_world_feature_support_register();

	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "world_features_status", WorldFeatures_Status_f );
		ri.Cmd_AddCommand( "world_features_validate", WorldFeatures_Validate_f );
		ri.Cmd_AddCommand( "world_feature_status", WorldFeature_Status_f );
		ri.Cmd_AddCommand( "world_feature_enable", WorldFeature_Enable_f );
		ri.Cmd_AddCommand( "world_feature_disable", WorldFeature_Disable_f );
		ri.Cmd_AddCommand( "world_perf_status", World_PerfStatus_f );
		ri.Cmd_AddCommand( "world_memory_status", World_MemoryStatus_f );
		s_cmds = qtrue;
	}

	ri.Printf( PRINT_ALL, "[VK][WorldPresentation] registry ready (mask=0x%08x)\n",
		vk_world_presentation_enabled_mask() );
}

#endif /* USE_VULKAN */
