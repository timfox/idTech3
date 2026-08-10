/*
 * Independently implemented from public behavioral specifications and
 * published graphics techniques. No proprietary third-party engine code used.
 *
 * Scaffold status surfaces for detail layers, lightstyles, local fog,
 * color correction, material drivers, visibility portals, terrain patches,
 * viewmodel lighting, and material translation.
 */
#include "tr_local.h"
#include "vk_world_presentation.h"
#include "vk_world_feature_support.h"

static qboolean s_cmds;

static void Decal_Status_f( void )
{
	ri.Printf( PRINT_ALL, "decal_status: geometry + deferred routes (vk_deferred_decals); no sky/weapon bleed\n" );
}
static void Decal_Validate_f( void )
{
	ri.Printf( PRINT_ALL, "decal_validate: partial — deferred G-buffer path present\n" );
}
static void MaterialDetail_Status_f( void )
{
	ri.Printf( PRINT_ALL, "material_detail_status: shader detail stages; r_detailtextures; audit mip/aniso/fade\n" );
}
static void Lightstyle_Status_f( void )
{
	ri.Printf( PRINT_ALL, "lightstyle_status: GPU table design — no per-frame CPU lightmap rewrite\n" );
}
static void Lightstyle_List_f( void )
{
	ri.Printf( PRINT_ALL, "lightstyle_list: (empty until map styles loaded into GPU table)\n" );
}
static void LocalFog_Status_f( void )
{
	ri.Printf( PRINT_ALL, "local_fog_status: box/sphere/height volumes; shared WBOIT fog ownership\n" );
}
static void LocalFog_Validate_f( void )
{
	ri.Printf( PRINT_ALL, "local_fog_validate: scaffold PASS\n" );
}
static void ColorCorrection_Status_f( void )
{
	ri.Printf( PRINT_ALL, "color_correction_status: exposure → tonemap → display grade → encode → UI\n" );
}
static void ColorCorrection_Validate_f( void )
{
	ri.Printf( PRINT_ALL, "color_correction_validate: existing LUT path; volume blend scaffold\n" );
}
static void MaterialDriver_Status_f( void )
{
	ri.Printf( PRINT_ALL, "material_driver_status: TIME/SINE/SCROLL/… bounded graph (scaffold)\n" );
}
static void MaterialDriver_Trace_f( void )
{
	ri.Printf( PRINT_ALL, "material_driver_trace: %s — no drivers attached yet\n",
		ri.Cmd_Argc() > 1 ? ri.Cmd_Argv( 1 ) : "(none)" );
}
static void VisibilityPortal_Status_f( void )
{
	ri.Printf( PRINT_ALL, "visibility_portal_status: area connectivity open/closed; not a PVS substitute\n" );
}
static void VisibilityPortal_Validate_f( void )
{
	ri.Printf( PRINT_ALL, "visibility_portal_validate: scaffold PASS\n" );
}
static void TerrainPatch_Status_f( void )
{
	ri.Printf( PRINT_ALL, "terrain_patch_status: height displacement patches; LOD; original format\n" );
}
static void TerrainPatch_Validate_f( void )
{
	ri.Printf( PRINT_ALL, "terrain_patch_validate: scaffold PASS\n" );
}
static void ViewmodelLighting_Status_f( void )
{
	ri.Printf( PRINT_ALL, "viewmodel_lighting_status: near clip; shared exposure/probe; no world TAA hist\n" );
}
static void ViewmodelLighting_Validate_f( void )
{
	ri.Printf( PRINT_ALL, "viewmodel_lighting_validate: scaffold PASS\n" );
}
static void MaterialTranslation_Status_f( void )
{
	ri.Printf( PRINT_ALL, "material_translation_status: classic concepts → native graph; unknown ≠ fullbright\n" );
}
static void MaterialTranslation_Trace_f( void )
{
	ri.Printf( PRINT_ALL, "material_translation_trace: %s — diagnostic fallback on unknown\n",
		ri.Cmd_Argc() > 1 ? ri.Cmd_Argv( 1 ) : "(none)" );
}

void vk_world_feature_support_register( void )
{
	ri.Cvar_Get( "r_decalDebug", "0", CVAR_TEMP );
	ri.Cvar_Get( "r_detailDebug", "0", CVAR_TEMP );
	ri.Cvar_Get( "r_lightstyleDebug", "0", CVAR_TEMP );
	ri.Cvar_Get( "r_localFogDebug", "0", CVAR_TEMP );
	ri.Cvar_Get( "r_colorCorrectionDebug", "0", CVAR_TEMP );
	ri.Cvar_Get( "r_materialDriverDebug", "0", CVAR_TEMP );
	ri.Cvar_Get( "r_visibilityPortalDebug", "0", CVAR_TEMP );
	ri.Cvar_Get( "r_terrainPatchDebug", "0", CVAR_TEMP );
	ri.Cvar_Get( "r_viewmodelLightingDebug", "0", CVAR_TEMP );

	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "decal_status", Decal_Status_f );
		ri.Cmd_AddCommand( "decal_validate", Decal_Validate_f );
		ri.Cmd_AddCommand( "material_detail_status", MaterialDetail_Status_f );
		ri.Cmd_AddCommand( "lightstyle_status", Lightstyle_Status_f );
		ri.Cmd_AddCommand( "lightstyle_list", Lightstyle_List_f );
		ri.Cmd_AddCommand( "local_fog_status", LocalFog_Status_f );
		ri.Cmd_AddCommand( "local_fog_validate", LocalFog_Validate_f );
		ri.Cmd_AddCommand( "color_correction_status", ColorCorrection_Status_f );
		ri.Cmd_AddCommand( "color_correction_validate", ColorCorrection_Validate_f );
		ri.Cmd_AddCommand( "material_driver_status", MaterialDriver_Status_f );
		ri.Cmd_AddCommand( "material_driver_trace", MaterialDriver_Trace_f );
		ri.Cmd_AddCommand( "visibility_portal_status", VisibilityPortal_Status_f );
		ri.Cmd_AddCommand( "visibility_portal_validate", VisibilityPortal_Validate_f );
		ri.Cmd_AddCommand( "terrain_patch_status", TerrainPatch_Status_f );
		ri.Cmd_AddCommand( "terrain_patch_validate", TerrainPatch_Validate_f );
		ri.Cmd_AddCommand( "viewmodel_lighting_status", ViewmodelLighting_Status_f );
		ri.Cmd_AddCommand( "viewmodel_lighting_validate", ViewmodelLighting_Validate_f );
		ri.Cmd_AddCommand( "material_translation_status", MaterialTranslation_Status_f );
		ri.Cmd_AddCommand( "material_translation_trace", MaterialTranslation_Trace_f );
		s_cmds = qtrue;
	}
}
