/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Client districts: FreeUSD manifest parse, console commands, view residency.
===========================================================================
*/

extern "C" {
#include "client.h"
#include "../world/world_district.h"
}

#ifdef USE_FREEUSD

#include <cstring>
#include <string>

#include "freeusd/c/freeusd.h"
#include "freeusd/usd/stage.hpp"
#include "freeusd/usdUtils/engineScene.hpp"

namespace {

static bool WD_IsDistrictAssembly( const freeusd::usdUtils::EngineSceneNode &node ) {
	const std::string path = node.path.GetString();
	if ( node.name.rfind( "District_", 0 ) != 0 &&
		path.find( "/Districts/" ) == std::string::npos &&
		path.find( "/District_" ) == std::string::npos ) {
		return false;
	}
	if ( node.has_prim_kind ) {
		const std::string kind = node.prim_kind.GetText();
		if ( kind == "assembly" || kind == "group" || kind == "component" ) {
			return true;
		}
	}
	return node.name.rfind( "District_", 0 ) == 0;
}

static void WD_BoundsFromNode( const freeusd::usdUtils::EngineSceneNode &node,
	vec3_t outMin, vec3_t outMax ) {
	const freeusd::gf::BBox3d &wb = node.world_bound;
	if ( !wb.IsEmpty() ) {
		outMin[0] = (float)wb.min.x();
		outMin[1] = (float)wb.min.y();
		outMin[2] = (float)wb.min.z();
		outMax[0] = (float)wb.max.x();
		outMax[1] = (float)wb.max.y();
		outMax[2] = (float)wb.max.z();
		return;
	}
	{
		const freeusd::gf::Matrix4d &m = node.local_to_world_transform;
		const double tx = m.m[12];
		const double ty = m.m[13];
		const double tz = m.m[14];
		outMin[0] = (float)tx - 512.0f;
		outMin[1] = (float)ty - 512.0f;
		outMin[2] = (float)tz - 64.0f;
		outMax[0] = (float)tx + 512.0f;
		outMax[1] = (float)ty + 512.0f;
		outMax[2] = (float)tz + 256.0f;
	}
}

}  // namespace

extern "C" qboolean WorldDistrict_ParseManifestFreeUSD( const char *osPath, const char *qpath,
	worldDistrict_t *out, int maxOut, int *outCount ) {
	std::string err;
	int count = 0;

	if ( !osPath || !out || !outCount || maxOut <= 0 ) {
		return qfalse;
	}

	auto stage = freeusd::usd::Stage::OpenFromRootFile(
		osPath, freeusd::usd::RootLayerSublayersPolicy::DepthFirst, &err );
	if ( !stage ) {
		Com_Printf( "[world_district] FreeUSD open failed: %s\n", err.c_str() );
		return qfalse;
	}

	const auto snap = freeusd::usdUtils::BuildEngineSceneSnapshot( *stage, 1.0 );

	for ( const auto &node : snap.nodes ) {
		if ( count >= maxOut ) {
			break;
		}
		if ( !WD_IsDistrictAssembly( node ) ) {
			continue;
		}
		if ( node.purpose == "proxy" ) {
			continue;
		}

		worldDistrict_t *d = &out[count];
		Com_Memset( d, 0, sizeof( *d ) );
		Q_strncpyz( d->name, node.name.c_str(), sizeof( d->name ) );
		if ( qpath ) {
			Q_strncpyz( d->manifestPath, qpath, sizeof( d->manifestPath ) );
		}
		WD_BoundsFromNode( node, d->boundsMin, d->boundsMax );
		d->active = qtrue;
		d->state = WD_STATE_UNLOADED;
		count++;
	}

	/* purpose=proxy prims: optional path override via prim name District_Foo_Proxy */
	for ( const auto &node : snap.nodes ) {
		if ( node.purpose != "proxy" ) {
			continue;
		}
		std::string proxyName = node.name;
		const std::size_t suffix = proxyName.rfind( "_Proxy" );
		if ( suffix == std::string::npos ) {
			continue;
		}
		proxyName = proxyName.substr( 0, suffix );
		for ( int i = 0; i < count; i++ ) {
			if ( !Q_stricmp( out[i].name, proxyName.c_str() ) ) {
				char slug[64];
				int j = 0;
				const char *base = out[i].name;
				if ( !Q_strncmp( base, "District_", 9 ) ) {
					base += 9;
				}
				for ( ; base[j] && j < (int)sizeof( slug ) - 1; j++ ) {
					char c = base[j];
					if ( c >= 'A' && c <= 'Z' ) {
						c = (char)( c - 'A' + 'a' );
					}
					slug[j] = c;
				}
				slug[j] = '\0';
				Com_sprintf( out[i].proxyMeshPath, sizeof( out[i].proxyMeshPath ),
					"world/proxies/%s_proxy.usda", slug );
				break;
			}
		}
	}

	*outCount = count;
	return count > 0 ? qtrue : qfalse;
}

#else

extern "C" qboolean WorldDistrict_ParseManifestFreeUSD( const char *osPath, const char *qpath,
	worldDistrict_t *out, int maxOut, int *outCount ) {
	(void)osPath;
	(void)qpath;
	(void)out;
	(void)maxOut;
	if ( outCount ) {
		*outCount = 0;
	}
	return qfalse;
}

#endif /* USE_FREEUSD */

static qhandle_t CL_District_RegisterModel( const char *path ) {
	if ( !path || !path[0] || !re.RegisterModel ) {
		return 0;
	}
	return re.RegisterModel( path );
}

static void CL_District_LoadManifest_f( void ) {
	worldDistrict_t parsed[WORLD_DISTRICT_MAX];
	int count = 0;
	const char *qpath;

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: district_load <world/playfield.usda>\n" );
		return;
	}
	qpath = Cmd_Argv( 1 );

#ifdef USE_FREEUSD
	{
		const char *base = Cvar_VariableString( "fs_basepath" );
		const char *game = Cvar_VariableString( "fs_game" );
		char *os = FS_BuildOSPath( base, game, qpath );
		if ( !os || !os[0] ) {
			Com_Printf( S_COLOR_YELLOW "[world_district] could not resolve '%s'\n", qpath );
			return;
		}
		if ( !WorldDistrict_ParseManifestFreeUSD( os, qpath, parsed, WORLD_DISTRICT_MAX, &count ) ) {
			Com_Printf( S_COLOR_YELLOW "[world_district] parse failed for '%s'\n", qpath );
			return;
		}
	}
#else
	Com_Printf( S_COLOR_YELLOW "[world_district] USE_FREEUSD build required\n" );
	return;
#endif

	WorldDistrict_Import( count, parsed, qpath );
}

static void CL_District_List_f( void ) {
	WorldDistrict_List();
}

static int CL_District_ResolveIndex( int argIndex ) {
	const char *arg;

	if ( Cmd_Argc() <= argIndex ) {
		Com_Printf( "Usage: %s <index|District_Name>\n", Cmd_Argv( 0 ) );
		return -1;
	}
	arg = Cmd_Argv( argIndex );
	if ( arg[0] >= '0' && arg[0] <= '9' ) {
		return atoi( arg );
	}
	return WorldDistrict_FindByName( arg );
}

static void CL_District_Status_f( void ) {
	int idx = ( Cmd_Argc() >= 2 ) ? CL_District_ResolveIndex( 1 ) : 0;
	if ( idx < 0 ) {
		return;
	}
	WorldDistrict_Status( idx );
}

static void CL_District_Proxy_f( void ) {
	int idx = CL_District_ResolveIndex( 1 );
	if ( idx < 0 ) {
		return;
	}
	WorldDistrict_LoadProxy( idx );
}

static void CL_District_LoadFull_f( void ) {
	int idx = CL_District_ResolveIndex( 1 );
	if ( idx < 0 ) {
		return;
	}
	WorldDistrict_LoadFull( idx );
}

static void CL_District_Unload_f( void ) {
	int idx = CL_District_ResolveIndex( 1 );
	if ( idx < 0 ) {
		return;
	}
	WorldDistrict_Unload( idx );
}

extern "C" void CL_District_Init( void ) {
	WorldDistrict_Init();
	WorldDistrict_SetRegisterModel( CL_District_RegisterModel );

	Cmd_AddCommand( "district_load", CL_District_LoadManifest_f );
	Cmd_AddCommand( "district_list", CL_District_List_f );
	Cmd_AddCommand( "district_status", CL_District_Status_f );
	Cmd_AddCommand( "district_proxy", CL_District_Proxy_f );
	Cmd_AddCommand( "district_load_full", CL_District_LoadFull_f );
	Cmd_AddCommand( "district_unload", CL_District_Unload_f );

	Com_Printf( "World districts: district_load, district_list, district_proxy (r_district 1)\n" );
}

extern "C" void CL_District_Frame( void ) {
	cvar_t *radius;

	if ( !cl.snap.valid ) {
		return;
	}
	radius = Cvar_Get( "r_districtLoadRadius", "8192", CVAR_ARCHIVE );
	WorldDistrict_UpdateView( cl.snap.ps.origin, radius ? radius->value : 8192.0f );
}
