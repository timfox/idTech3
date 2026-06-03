/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Console commands for FreeUSD scene inspection (entities, materials, runtime assess).
https://github.com/gopexllc/FreeUSD
===========================================================================
*/

extern "C" {
#include "client.h"
}
#include "cl_usd.h"

#ifdef USE_FREEUSD

#include <memory>
#include <string>

#include "freeusd/c/freeusd.h"
#include "freeusd/sdf/path.hpp"
#include "freeusd/usd/stage.hpp"
#include "freeusd/usdGeom/mesh.hpp"
#include "freeusd/usdShade/material.hpp"
#include "freeusd/usdShade/previewSurface.hpp"
#include "freeusd/usdUtils/engineScene.hpp"

namespace {

static int CL_USD_CountMeshTris( const freeusd::usdGeom::Mesh &mesh, double time ) {
	const std::vector<int> faceCounts = mesh.GetFaceVertexCounts( time );
	int tris = 0;
	for ( int c : faceCounts ) {
		if ( c >= 3 ) {
			tris += c - 2;
		}
	}
	return tris;
}

}  // namespace

static cvar_t *com_freeusd;
static cvar_t *com_usdEntities;
static cvar_t *com_usdShaders;

static std::string CL_USD_OSPath( const char *qpath ) {
	const char *base = Cvar_VariableString( "fs_basepath" );
	const char *game = Cvar_VariableString( "fs_game" );
	char *os = FS_BuildOSPath( base, game, qpath );

	if ( !os || !os[0] ) {
		return {};
	}
	return std::string( os );
}

static std::shared_ptr<freeusd::usd::Stage> CL_USD_OpenArg( void ) {
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: %s <path.usda>\n", Cmd_Argv( 0 ) );
		return nullptr;
	}
	if ( !com_freeusd || !com_freeusd->integer ) {
		Com_Printf( "com_freeusd is 0 — enable FreeUSD tools first\n" );
		return nullptr;
	}

	{
		const char *qpath = Cmd_Argv( 1 );
		std::string os = CL_USD_OSPath( qpath );
		std::string err;

		if ( os.empty() ) {
			Com_Printf( "Could not resolve OS path for '%s'\n", qpath );
			return nullptr;
		}

		auto stage = freeusd::usd::Stage::OpenFromRootFile(
			os, freeusd::usd::RootLayerSublayersPolicy::DepthFirst, &err );
		if ( !stage ) {
			Com_Printf( "FreeUSD open failed: %s\n", err.c_str() );
			return nullptr;
		}
		Com_Printf( "FreeUSD: opened %s (%s)\n", qpath, freeusd_version_string() );
		return stage;
	}
}

static void CL_USD_Info_f( void ) {
	auto stage = CL_USD_OpenArg();
	if ( !stage ) {
		return;
	}
	const auto snap = freeusd::usdUtils::BuildEngineSceneSnapshot( *stage, 1.0 );
	Com_Printf( "defaultPrim: %s\n", snap.default_prim_name.c_str() );
	Com_Printf( "prims: %zu  materials: %zu  lux lights: %zu  meshes(skel-bound): %zu\n",
		snap.prim_order.size(), snap.material_paths.size(), snap.lux_light_paths.size(),
		snap.skel_bound_geom_paths.size() );
	if ( snap.meters_per_unit.has_value() ) {
		Com_Printf( "metersPerUnit: %g\n", *snap.meters_per_unit );
	}
	if ( snap.up_axis.has_value() ) {
		Com_Printf( "upAxis: %s\n", snap.up_axis->c_str() );
	}
}

static void CL_USD_Assess_f( void ) {
	auto stage = CL_USD_OpenArg();
	if ( !stage ) {
		return;
	}
	const auto report = freeusd::usdUtils::AssessEngineRuntimeSupport( *stage );
	Com_Printf( "recommended runtime mode: %s\n",
		std::string( freeusd::usdUtils::EngineRuntimeModeName( report.recommended_mode ) ).c_str() );
	Com_Printf( "layerStack=%d refs=%d payloads=%d inherits=%d specializes=%d variants=%d timeSamples=%d\n",
		report.uses_composed_layer_stack ? 1 : 0,
		report.uses_references ? 1 : 0,
		report.uses_payloads ? 1 : 0,
		report.uses_inherits ? 1 : 0,
		report.uses_specializes ? 1 : 0,
		report.uses_variant_selection ? 1 : 0,
		report.uses_time_samples ? 1 : 0 );
	Com_Printf( "previewSurface=%d textures=%d lux=%d physics=%d openVDB=%d\n",
		report.uses_preview_surface ? 1 : 0,
		report.uses_preview_surface_textures ? 1 : 0,
		report.uses_lux_lights ? 1 : 0,
		report.uses_physics_scenes ? 1 : 0,
		report.uses_open_vdb_assets ? 1 : 0 );
	for ( const auto &w : report.warnings ) {
		Com_Printf( S_COLOR_YELLOW "warning: %s\n", w.c_str() );
	}
}

static void CL_USD_Entities_f( void ) {
	if ( !com_usdEntities || !com_usdEntities->integer ) {
		Com_Printf( "com_usdEntities is 0\n" );
		return;
	}
	auto stage = CL_USD_OpenArg();
	if ( !stage ) {
		return;
	}
	const auto snap = freeusd::usdUtils::BuildEngineSceneSnapshot( *stage, 1.0 );
	Com_Printf( "--- USD scene prims (entity candidates) ---\n" );
	for ( const auto &node : snap.nodes ) {
		if ( !node.active || !node.visible ) {
			continue;
		}
		Com_Printf( "%s  kind=%s  children=%zu\n",
			node.path.GetString().c_str(),
			node.prim_kind.GetText().c_str(),
			node.child_paths.size() );
	}
}

static void CL_USD_Meshes_f( void ) {
	auto stage = CL_USD_OpenArg();
	if ( !stage ) {
		return;
	}
	const double time = atof( Cvar_VariableString( "r_freeusdTime" ) );
	const auto snap = freeusd::usdUtils::BuildEngineSceneSnapshot( *stage, time );
	int index = 0;

	Com_Printf( "--- UsdGeom.Mesh prims (index for r_freeusdMeshIndex) ---\n" );
	for ( const auto &path : snap.prim_order ) {
		freeusd::usdGeom::Mesh mesh( stage->GetPrimAtPath( path ) );
		int numTris;

		if ( !mesh || mesh.GetPoints( time ).empty() ) {
			continue;
		}
		numTris = CL_USD_CountMeshTris( mesh, time );
		if ( numTris <= 0 ) {
			continue;
		}
		Com_Printf( "  [%d] %s  tris=%d\n", index, path.GetString().c_str(), numTris );
		index++;
	}
	if ( index == 0 ) {
		Com_Printf( "  (no tessellatable meshes)\n" );
	}
}

static void CL_USD_AssetToShaderQpath( const std::string &asset, char *out, int outSize ) {
	std::string p = asset;
	size_t dot;

	while ( !p.empty() && ( p.front() == '@' || p.front() == ' ' ) ) {
		p.erase( p.begin() );
	}
	while ( !p.empty() && ( p.back() == '@' || p.back() == ' ' ) ) {
		p.pop_back();
	}
	if ( p.size() >= 2 && p[0] == '.' && p[1] == '/' ) {
		p = p.substr( 2 );
	}
	dot = p.find_last_of( '.' );
	if ( dot != std::string::npos ) {
		p = p.substr( 0, dot );
	}
	if ( p.empty() || outSize < 2 ) {
		return;
	}
	Q_strncpyz( out, p.c_str(), outSize );
}

static void CL_USD_ShaderMap_f( void ) {
	auto stage = CL_USD_OpenArg();
	if ( !stage ) {
		return;
	}
	const double time = atof( Cvar_VariableString( "r_freeusdTime" ) );
	const auto snap = freeusd::usdUtils::BuildEngineSceneSnapshot( *stage, time );
	char shaderQpath[MAX_QPATH];

	Com_Printf( "--- mesh material -> Q3 shader (r_freeusdShaderMap) ---\n" );
	for ( const auto &node : snap.nodes ) {
		freeusd::usdGeom::Mesh mesh( stage->GetPrimAtPath( node.path ) );
		std::string texPath;
		freeusd::usdShade::PreviewSurface preview;

		if ( !mesh ) {
			continue;
		}
		Q_strncpyz( shaderQpath, "white", sizeof( shaderQpath ) );
		if ( !node.has_material_binding || node.material_path.IsEmpty() ) {
			Com_Printf( "  %s -> %s (no binding)\n", node.path.GetString().c_str(), shaderQpath );
			continue;
		}

		{
			freeusd::usdShade::Material material =
				freeusd::usdShade::Material::ReadFromPrim( stage, node.material_path );
			freeusd::sdf::Path surfaceShader;
			if ( material ) {
				surfaceShader = material.GetSurfaceShaderPath();
				if ( !surfaceShader.IsEmpty() ) {
					preview = freeusd::usdShade::PreviewSurface::ReadFromPrim( stage, surfaceShader );
				}
			}
		}

		if ( preview && preview.GetDiffuseTextureAssetPath( &texPath, time ) && !texPath.empty() ) {
			CL_USD_AssetToShaderQpath( texPath, shaderQpath, sizeof( shaderQpath ) );
		} else if ( preview ) {
			Q_strncpyz( shaderQpath, "white", sizeof( shaderQpath ) );
		}

		Com_Printf( "  %s -> %s (mat %s)\n", node.path.GetString().c_str(), shaderQpath,
			node.material_path.GetString().c_str() );
	}
}

static void CL_USD_Load_f( void ) {
	if ( !com_freeusd || !com_freeusd->integer ) {
		Com_Printf( "com_freeusd is 0 — enable FreeUSD tools first\n" );
		return;
	}
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: usd_load <path.usda> [meshIndex] — registers model (r_freeusd 1)\n" );
		return;
	}
	if ( !re.RegisterModel ) {
		Com_Printf( "Renderer not ready\n" );
		return;
	}
	if ( Cmd_Argc() >= 3 ) {
		Cvar_Set( "r_freeusdPickLargest", "0" );
		Cvar_SetValue( "r_freeusdMeshIndex", (float)atoi( Cmd_Argv( 2 ) ) );
	}
	{
		const char *qpath = Cmd_Argv( 1 );
		qhandle_t h = re.RegisterModel( qpath );
		if ( h ) {
			Com_Printf( "usd_load: registered model %s (handle %d)\n", qpath, h );
		} else {
			Com_Printf( S_COLOR_YELLOW "usd_load: failed to register '%s' (check path, r_freeusd, FreeUSD build)\n",
				qpath );
		}
	}
}

static void CL_USD_Shaders_f( void ) {
	if ( !com_usdShaders || !com_usdShaders->integer ) {
		Com_Printf( "com_usdShaders is 0\n" );
		return;
	}
	auto stage = CL_USD_OpenArg();
	if ( !stage ) {
		return;
	}
	const auto snap = freeusd::usdUtils::BuildEngineSceneSnapshot( *stage, 1.0 );
	size_t i;

	Com_Printf( "--- UsdPreviewSurface materials ---\n" );
	for ( i = 0; i < snap.material_paths.size(); i++ ) {
		Com_Printf( "  %s\n", snap.material_paths[i].GetString().c_str() );
	}
	Com_Printf( "--- textured preview shaders ---\n" );
	for ( i = 0; i < snap.preview_surface_textured_shader_paths.size(); i++ ) {
		Com_Printf( "  %s\n", snap.preview_surface_textured_shader_paths[i].GetString().c_str() );
	}
	Com_Printf( "--- material bindings (geom) ---\n" );
	for ( i = 0; i < snap.material_bound_geom_paths.size(); i++ ) {
		const auto *n = [&]() -> const freeusd::usdUtils::EngineSceneNode * {
			for ( const auto &node : snap.nodes ) {
				if ( node.path == snap.material_bound_geom_paths[i] ) {
					return &node;
				}
			}
			return nullptr;
		}();
		if ( n && n->has_material_binding ) {
			Com_Printf( "  geom %s -> %s\n",
				snap.material_bound_geom_paths[i].GetString().c_str(),
				n->material_path.GetString().c_str() );
		}
	}
}

#endif /* USE_FREEUSD */

extern "C" void CL_USD_Init( void ) {
	com_freeusd = Cvar_Get( "com_freeusd", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( com_freeusd,
		"Enable FreeUSD client tools (usd_info, usd_assess, usd_entities, usd_shaders, usd_meshes, usd_load). Requires USE_FREEUSD build." );

	com_usdEntities = Cvar_Get( "com_usdEntities", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( com_usdEntities,
		"When 1, usd_entities lists composed prim hierarchy for map/entity authoring." );

	com_usdShaders = Cvar_Get( "com_usdShaders", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( com_usdShaders,
		"When 1, usd_shaders lists UsdPreviewSurface materials and geom bindings." );

#ifdef USE_FREEUSD
	Com_Printf( "FreeUSD client tools enabled (%s)\n", freeusd_version_string() );
	Cmd_AddCommand( "usd_info", CL_USD_Info_f );
	Cmd_AddCommand( "usd_assess", CL_USD_Assess_f );
	Cmd_AddCommand( "usd_entities", CL_USD_Entities_f );
	Cmd_AddCommand( "usd_shaders", CL_USD_Shaders_f );
	Cmd_AddCommand( "usd_meshes", CL_USD_Meshes_f );
	Cmd_AddCommand( "usd_shader_map", CL_USD_ShaderMap_f );
	Cmd_AddCommand( "usd_load", CL_USD_Load_f );
#else
	Com_Printf( "FreeUSD: not built (cmake -DUSE_FREEUSD=ON)\n" );
#endif
}
