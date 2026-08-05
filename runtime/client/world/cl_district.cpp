/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Client districts: FreeUSD manifest parse, console commands, view residency.
===========================================================================
*/

extern "C" {
#include "client.h"
#include "../../world/world_district.h"
#include "../../world/world_zone.h"
#include "defer.h"
#include "jobs.h"
}

#ifdef USE_FREEUSD

#include "cl_freeusd_util.hpp"
#include <cstring>
#include <cstdint>
#include <new>
#include <string>

#include "freeusd/c/freeusd.h"
#include "freeusd/usd/stage.hpp"
#include "freeusd/usdUtils/engineScene.hpp"

namespace {

struct DistrictManifestJob {
	char qpath[WORLD_DISTRICT_PATH_MAX];
	char osPath[1024];
	worldDistrict_t parsed[WORLD_DISTRICT_MAX];
	int count;
	qboolean ok;
};

static qboolean s_districtManifestBusy = qfalse;

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
	worldDistrict_t *out, int maxOut, int *outCount );

static void CL_District_ApplyManifestJob( void *data ) {
	DistrictManifestJob *job = (DistrictManifestJob *)data;
	if ( !job ) {
		s_districtManifestBusy = qfalse;
		return;
	}
	s_districtManifestBusy = qfalse;
	if ( job->ok ) {
		WorldDistrict_Import( job->count, job->parsed, job->qpath );
		Com_Printf( "[world_district] async manifest ready: %s (%d district(s))\n",
			job->qpath, job->count );
	} else {
		Com_Printf( S_COLOR_YELLOW "[world_district] async manifest failed: %s\n", job->qpath );
	}
	delete job;
}

static void CL_District_ParseManifestJob( void *data, uint32_t count ) {
	DistrictManifestJob *job = (DistrictManifestJob *)data;
	(void)count;
	if ( !job ) {
		return;
	}
	job->count = 0;
	job->ok = WorldDistrict_ParseManifestFreeUSD( job->osPath, job->qpath,
		job->parsed, WORLD_DISTRICT_MAX, &job->count );
	if ( !Defer_Add( CL_District_ApplyManifestJob, job ) ) {
		delete job;
		s_districtManifestBusy = qfalse;
	}
}

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
		d->origin[0] = (float)node.local_to_world_transform.m[12];
		d->origin[1] = (float)node.local_to_world_transform.m[13];
		d->origin[2] = (float)node.local_to_world_transform.m[14];
		d->zoneResidencyMask = WORLD_ZONE_RESIDENCY_ALL;
		d->zonePriority = 1.0f;
		{
			const auto prim = stage->GetPrimAtPath( node.path );
			double value;
			std::int32_t mask;
			if ( prim.IsValid() && prim.HasCustomDataKey( "zoneLoadRadius" ) &&
				prim.GetCustomData( "zoneLoadRadius" ).GetDouble( &value ) ) d->zoneLoadRadius = (float)value;
			if ( prim.IsValid() && prim.HasCustomDataKey( "zoneUnloadRadius" ) &&
				prim.GetCustomData( "zoneUnloadRadius" ).GetDouble( &value ) ) d->zoneUnloadRadius = (float)value;
			if ( prim.IsValid() && prim.HasCustomDataKey( "zonePriority" ) &&
				prim.GetCustomData( "zonePriority" ).GetDouble( &value ) ) d->zonePriority = (float)value;
			if ( prim.IsValid() && prim.HasCustomDataKey( "residencyMask" ) &&
				prim.GetCustomData( "residencyMask" ).GetInt32( &mask ) ) d->zoneResidencyMask = (uint32_t)mask;
		}
		/*
		 * A validation manifest may point at a large external payload without
		 * copying it into the game tree.  Keep the conventional slug fallback
		 * in WorldDistrict_DefaultPaths, but honor an explicit authored path
		 * when present.  This is also the ownership boundary for benchmark
		 * fixtures: the manifest owns which full scene is loaded.
		 */
		{
			const auto prim = stage->GetPrimAtPath( node.path );
			std::string fullMeshPath;
			if ( prim.IsValid() && prim.HasCustomDataKey( "fullMesh" ) &&
				prim.GetCustomData( "fullMesh" ).GetString( &fullMeshPath ) &&
				!fullMeshPath.empty() ) {
				Q_strncpyz( d->fullMeshPath, fullMeshPath.c_str(), sizeof( d->fullMeshPath ) );
			}
		}
		/* The lightweight manifest often has no authored bounds. Resolve the
		 * explicit payload now and use its composed mesh bounds for camera and
		 * residency decisions; the generic 1024-unit fallback is only for
		 * manifests that genuinely have no payload metadata. */
		if ( d->fullMeshPath[0] ) {
			std::string payloadErr;
			const std::string payloadOs = Cl_FreeusdBuildOsPath( d->fullMeshPath );
			if ( !payloadOs.empty() ) {
				auto payloadStage = freeusd::usd::Stage::OpenFromRootFile(
					payloadOs, freeusd::usd::RootLayerSublayersPolicy::DepthFirst, &payloadErr );
				if ( payloadStage ) {
					const auto payloadSnap = freeusd::usdUtils::BuildEngineSceneSnapshot( *payloadStage, 1.0 );
					freeusd::gf::BBox3d payloadBounds = freeusd::gf::BBox3d::Empty();
					for ( const auto &payloadNode : payloadSnap.nodes ) {
						if ( !payloadNode.world_bound.IsEmpty() ) {
							payloadBounds = freeusd::gf::BBox3d::Union( payloadBounds, payloadNode.world_bound );
						}
					}
					if ( !payloadBounds.IsEmpty() ) {
						d->boundsMin[0] = (float)payloadBounds.min.x();
						d->boundsMin[1] = (float)payloadBounds.min.y();
						d->boundsMin[2] = (float)payloadBounds.min.z();
						d->boundsMax[0] = (float)payloadBounds.max.x();
						d->boundsMax[1] = (float)payloadBounds.max.y();
						d->boundsMax[2] = (float)payloadBounds.max.z();
						Com_Printf( "[world_district] %s payload bounds: (%.0f,%.0f,%.0f)..(%.0f,%.0f,%.0f)\n",
							d->name, d->boundsMin[0], d->boundsMin[1], d->boundsMin[2],
							d->boundsMax[0], d->boundsMax[1], d->boundsMax[2] );
					}
				}
			}
		}
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

static void CL_District_OnUnload( int index, const worldDistrict_t *d ) {
	int x, y;

	(void)index;
	if ( !d || !re.BspStreamUnmergeSector ) {
		return;
	}
	for ( y = d->sectorY0; y <= d->sectorY1; y++ ) {
		for ( x = d->sectorX0; x <= d->sectorX1; x++ ) {
			re.BspStreamUnmergeSector( x, y );
		}
	}
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
	if ( Cvar_VariableIntegerValue( "r_districtAsyncLoad" ) ) {
		if ( s_districtManifestBusy ) {
			Com_Printf( "[world_district] manifest load already in progress\n" );
			return;
		}
#ifdef USE_FREEUSD
		{
			std::string os = Cl_FreeusdBuildOsPath( qpath );
			DistrictManifestJob *job;
			job = new ( std::nothrow ) DistrictManifestJob();
			if ( !job || os.empty() ) {
				delete job;
				Com_Printf( S_COLOR_YELLOW "[world_district] async manifest allocation/path failure: %s\n", qpath );
				return;
			}
			Com_Memset( job, 0, sizeof( *job ) );
			Q_strncpyz( job->qpath, qpath, sizeof( job->qpath ) );
			Q_strncpyz( job->osPath, os.c_str(), sizeof( job->osPath ) );
			s_districtManifestBusy = qtrue;
			if ( Jobs_SubmitWork( CL_District_ParseManifestJob, job, JOB_PRIORITY_LOW ) == JOBS_INVALID_HANDLE ) {
				s_districtManifestBusy = qfalse;
				delete job;
				Com_Printf( S_COLOR_YELLOW "[world_district] async queue unavailable; retrying synchronously\n" );
			} else {
				Com_Printf( "[world_district] queued async manifest load: %s\n", qpath );
				return;
			}
		}
#endif
	}

#ifdef USE_FREEUSD
	{
		std::string os = Cl_FreeusdBuildOsPath( qpath );
		if ( os.empty() ) {
			Com_Printf( S_COLOR_YELLOW "[world_district] could not resolve '%s'\n", qpath );
			return;
		}
		if ( !WorldDistrict_ParseManifestFreeUSD( os.c_str(), qpath, parsed, WORLD_DISTRICT_MAX, &count ) ) {
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
	WorldDistrict_SetOnUnload( CL_District_OnUnload );

	Cmd_AddCommand( "district_load", CL_District_LoadManifest_f );
	Cmd_AddCommand( "district_list", CL_District_List_f );
	Cmd_AddCommand( "district_status", CL_District_Status_f );
	Cmd_AddCommand( "district_proxy", CL_District_Proxy_f );
	Cmd_AddCommand( "district_load_full", CL_District_LoadFull_f );
	Cmd_AddCommand( "district_unload", CL_District_Unload_f );

	Cvar_SetDescription( Cvar_Get( "r_districtDraw", "1", CVAR_ARCHIVE ),
		"When 1, draw loaded district proxy/full FreeUSD meshes at manifest origins each frame." );
	Cvar_SetDescription( Cvar_Get( "r_districtAnchorView", "0", CVAR_ARCHIVE ),
		"Validation mode: place each loaded district bounds center at the current camera. "
		"Use for USDA proof captures; leaves authored world placement unchanged when 0." );
	Cvar_SetDescription( Cvar_Get( "r_districtCamera", "0", CVAR_ARCHIVE ),
		"Validation camera: replace the gameplay view with an automatic camera aimed at the loaded district bounds. "
		"This makes USDA proof captures independent of the active BSP map." );
	Cvar_SetDescription( Cvar_Get( "r_districtOnly", "0", CVAR_ARCHIVE ),
		"Validation scene mode: hide the active BSP world while rendering loaded district entities." );
	Cvar_SetDescription( Cvar_Get( "r_districtCameraDistance", "1.0", CVAR_ARCHIVE ),
		"Validation camera distance in district extents; lower values move the USDA proof camera into the scene." );
	Cvar_SetDescription( Cvar_Get( "r_districtAsyncLoad", "1", CVAR_ARCHIVE ),
		"Parse USDA district manifests on the engine job pool and commit them on the main thread." );

	Com_Printf( "World districts: district_load, district_list, district_proxy (r_district 1, r_districtDraw 1)\n" );
}

extern "C" void CL_District_ApplyView( refdef_t *fd ) {
	const worldDistrict_t *d;
	vec3_t center, half, eye, direction, angles;
	float extent, distanceScale;

	if ( !fd || !Cvar_VariableIntegerValue( "r_districtCamera" ) ) {
		return;
	}
	d = WorldDistrict_Get( 0 );
	if ( !d || !d->active ) {
		return;
	}

	VectorAdd( d->boundsMin, d->boundsMax, center );
	VectorScale( center, 0.5f, center );
	VectorSubtract( d->boundsMax, d->boundsMin, half );
	extent = MAX( half[0], MAX( half[1], half[2] ) );
	if ( extent < 64.0f ) {
		extent = 512.0f;
	}
	distanceScale = Cvar_Get( "r_districtCameraDistance", "1.0", CVAR_ARCHIVE )->value;
	if ( distanceScale < 0.1f ) {
		distanceScale = 0.1f;
	}

	/* The default fixture is Z-up and its long axis is Y. Keep this camera
	 * derived from the manifest bounds so it remains useful for other USDA
	 * districts instead of baking OpenArena spawn coordinates into the proof. */
	VectorCopy( center, eye );
	eye[1] -= extent * distanceScale;
	eye[2] += extent * 0.05f;
	VectorSubtract( center, eye, direction );
	vectoangles( direction, angles );
	AnglesToAxis( angles, fd->viewaxis );
	VectorCopy( eye, fd->vieworg );
	if ( Cvar_VariableIntegerValue( "r_districtOnly" ) ) {
		/* Keep the host BSP visible until a district model has actually been
		 * committed. A failed/missing proxy must not turn the frame black while
		 * asynchronous or explicit full residency is still pending. */
		if ( ( d->proxyModel || d->fullModel ) &&
			( d->state == WD_STATE_PROXY || d->state == WD_STATE_STREAMING ||
			  d->state == WD_STATE_LOADED ) ) {
			fd->rdflags |= RDF_NOWORLDMODEL;
		}
	}
}

extern "C" void CL_District_Frame( void ) {
	cvar_t *radius;

	if ( !cl.snap.valid ) {
		return;
	}
	radius = Cvar_Get( "r_districtLoadRadius", "8192", CVAR_ARCHIVE );
	WorldDistrict_UpdateView( cl.snap.ps.origin, radius ? radius->value : 8192.0f );
}


extern "C" void CL_District_AddRefEntitiesToScene( void ) {
	cvar_t *districtDraw;
	int i;

	if ( !Cvar_VariableIntegerValue( "r_district" ) ) {
		return;
	}
	districtDraw = Cvar_Get( "r_districtDraw", "1", CVAR_ARCHIVE );
	if ( !districtDraw || !districtDraw->integer ) {
		return;
	}

	for ( i = 0; i < WorldDistrict_GetCount(); i++ ) {
		const worldDistrict_t *d = WorldDistrict_Get( i );
		refEntity_t ent;
		qhandle_t model = 0;
		vec3_t drawOrigin;

		if ( !d ) {
			continue;
		}
		switch ( d->state ) {
		case WD_STATE_PROXY:
			model = d->proxyModel;
			break;
		case WD_STATE_STREAMING:
		case WD_STATE_LOADED:
			model = d->fullModel ? d->fullModel : d->proxyModel;
			break;
		default:
			continue;
		}
		if ( !model ) {
			continue;
		}

		Com_Memset( &ent, 0, sizeof( ent ) );
		ent.reType = RT_MODEL;
		ent.hModel = model;
		VectorCopy( d->origin, drawOrigin );
		if ( Cvar_VariableIntegerValue( "r_districtAnchorView" ) ) {
			vec3_t center;
			VectorAdd( d->boundsMin, d->boundsMax, center );
			VectorScale( center, 0.5f, center );
			VectorSubtract( cl.snap.ps.origin, center, drawOrigin );
		}
		VectorCopy( drawOrigin, ent.origin );
		ent.axis[0][0] = 1.0f;
		ent.axis[1][1] = 1.0f;
		ent.axis[2][2] = 1.0f;
		ent.renderfx = RF_NOSHADOW;
		re.AddRefEntityToScene( &ent, qfalse );
	}
}

extern "C" void CL_District_RenderStandalone( void ) {
	refdef_t fd;

	if ( !Cvar_VariableIntegerValue( "r_district" ) ||
		!Cvar_VariableIntegerValue( "r_districtCamera" ) ||
		!WorldDistrict_GetCount() ) {
		return;
	}

	/* Native validation packages may intentionally ship without a cgame VM.
	 * In that case there is no CG_R_RENDERSCENE trap to reach the client
	 * RenderScene wrapper. Keep submission engine-owned while using the same
	 * wrapper as a cgame render. */
	Com_Memset( &fd, 0, sizeof( fd ) );
	fd.width = cls.glconfig.vidWidth;
	fd.height = cls.glconfig.vidHeight;
	fd.fov_x = 90.0f;
	fd.fov_y = 60.0f;
	fd.time = cls.realtime;
	CL_District_ApplyView( &fd );

	re.ClearScene();
	re.RenderScene( &fd );
}
