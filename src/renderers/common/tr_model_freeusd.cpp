/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

USD / USDA mesh tessellation via FreeUSD (C++ only; no tr_local.h).
===========================================================================
*/

#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

extern "C" {
#include "../../qcommon/q_shared.h"
#include "../../qcommon/qfiles.h"
#include "tr_model_freeusd.h"
#include "tr_public.h"
}

#ifdef USE_FREEUSD

#include "freeusd/gf/matrix4d.hpp"
#include "freeusd/gf/vec3f.hpp"
#include "freeusd/sdf/path.hpp"
#include "freeusd/usd/stage.hpp"
#include "freeusd/usdGeom/mesh.hpp"
#include "freeusd/usdShade/material.hpp"
#include "freeusd/usdShade/previewSurface.hpp"
#include "freeusd/usdUtils/engineScene.hpp"

namespace {

static double R_Freeusd_TimeCode( void ) {
	const char *s = ri.Cvar_VariableString( "r_freeusdTime" );
	if ( !s || !s[0] ) {
		return 1.0;
	}
	return (double)atof( s );
}

static std::string R_Freeusd_BuildOSPath( const char *qpath ) {
	const char *base = ri.Cvar_VariableString( "fs_basepath" );
	const char *game = ri.Cvar_VariableString( "fs_game" );
	char *os;

	if ( !qpath || !qpath[0] || !ri.FS_BuildOSPath ) {
		return {};
	}
	os = ri.FS_BuildOSPath( base, game, qpath );
	if ( !os || !os[0] ) {
		return {};
	}
	return std::string( os );
}

static void R_Freeusd_TransformPoint( const freeusd::gf::Matrix4d &m, float lx, float ly, float lz,
	float *ox, float *oy, float *oz ) {
	const double x = static_cast<double>( lx );
	const double y = static_cast<double>( ly );
	const double z = static_cast<double>( lz );
	*ox = static_cast<float>( x * m.m[0] + y * m.m[4] + z * m.m[8] + m.m[12] );
	*oy = static_cast<float>( x * m.m[1] + y * m.m[5] + z * m.m[9] + m.m[13] );
	*oz = static_cast<float>( x * m.m[2] + y * m.m[6] + z * m.m[10] + m.m[14] );
}

static const freeusd::usdUtils::EngineSceneNode *R_Freeusd_FindNode(
	const freeusd::usdUtils::EngineSceneSnapshot &snap, const freeusd::sdf::Path &path ) {
	for ( const auto &n : snap.nodes ) {
		if ( n.path == path ) {
			return &n;
		}
	}
	return nullptr;
}

static int R_Freeusd_CountMeshTris( const freeusd::usdGeom::Mesh &mesh, double time ) {
	const std::vector<int> faceCounts = mesh.GetFaceVertexCounts( time );
	int tris = 0;
	for ( int c : faceCounts ) {
		if ( c >= 3 ) {
			tris += c - 2;
		}
	}
	return tris;
}

struct FreeusdMeshCandidate {
	freeusd::sdf::Path path;
	int numTris;
};

static std::vector<FreeusdMeshCandidate> R_Freeusd_ListMeshCandidates(
	std::shared_ptr<freeusd::usd::Stage> stage,
	const freeusd::usdUtils::EngineSceneSnapshot &snap, double time ) {
	std::vector<FreeusdMeshCandidate> out;
	const char *pathFilter = ri.Cvar_VariableString( "r_freeusdMeshPath" );

	for ( const auto &path : snap.prim_order ) {
		freeusd::usdGeom::Mesh mesh( stage->GetPrimAtPath( path ) );
		int numTris;

		if ( !mesh || mesh.GetPoints( time ).empty() ) {
			continue;
		}
		numTris = R_Freeusd_CountMeshTris( mesh, time );
		if ( numTris <= 0 ) {
			continue;
		}
		if ( pathFilter && pathFilter[0] ) {
			if ( path.GetString().find( pathFilter ) == std::string::npos ) {
				continue;
			}
		}
		out.push_back( { path, numTris } );
	}
	return out;
}

static freeusd::sdf::Path R_Freeusd_ChooseMeshPath(
	std::shared_ptr<freeusd::usd::Stage> stage,
	const freeusd::usdUtils::EngineSceneSnapshot &snap, double time ) {
	const std::vector<FreeusdMeshCandidate> candidates = R_Freeusd_ListMeshCandidates( stage, snap, time );
	size_t pickIdx = 0;

	if ( candidates.empty() ) {
		return {};
	}

	if ( ri.Cvar_VariableIntegerValue( "r_freeusdPickLargest" ) ) {
		int bestTris = -1;
		for ( size_t i = 0; i < candidates.size(); i++ ) {
			if ( candidates[i].numTris > bestTris ) {
				bestTris = candidates[i].numTris;
				pickIdx = i;
			}
		}
	} else {
		int idx = ri.Cvar_VariableIntegerValue( "r_freeusdMeshIndex" );
		if ( idx < 0 ) {
			idx = 0;
		}
		if ( (size_t)idx >= candidates.size() ) {
			idx = (int)candidates.size() - 1;
		}
		pickIdx = (size_t)idx;
	}

	ri.Printf( PRINT_DEVELOPER, "FreeUSD: selected mesh %s (%d tris, index %zu of %zu)\n",
		candidates[pickIdx].path.GetString().c_str(), candidates[pickIdx].numTris, pickIdx,
		candidates.size() );

	return candidates[pickIdx].path;
}

static void R_Freeusd_AssetPathToShaderQpath( const std::string &asset, char *out, size_t outSize ) {
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
	Q_strncpyz( out, p.c_str(), (int)outSize );
}

static void R_Freeusd_ResolveShaderForMesh( std::shared_ptr<freeusd::usd::Stage> stage,
	const freeusd::usdUtils::EngineSceneNode *node, double time,
	char *out, size_t outSize ) {
	std::string texPath;
	freeusd::usdShade::Material material;
	freeusd::usdShade::PreviewSurface preview;
	freeusd::gf::Vec3f diffuse;
	freeusd::sdf::Path surfaceShader;

	if ( !out || outSize < 2 ) {
		return;
	}
	/* Default matches idtech3_demo.pk3 (demo_bootstrap.shader "white"); generic bases often use textures/common/white via mesh-import fallback when unbound. */
	Q_strncpyz( out, "white", (int)outSize );

	if ( !ri.Cvar_VariableIntegerValue( "r_freeusdShaderMap" ) ) {
		return;
	}
	if ( !node || !node->has_material_binding || node->material_path.IsEmpty() ) {
		return;
	}

	material = freeusd::usdShade::Material::ReadFromPrim( stage, node->material_path );
	if ( !material ) {
		return;
	}

	surfaceShader = material.GetSurfaceShaderPath();
	if ( surfaceShader.IsEmpty() ) {
		const auto shaders = material.ListShaderPrimPaths();
		for ( const auto &sp : shaders ) {
			preview = freeusd::usdShade::PreviewSurface::ReadFromPrim( stage, sp );
			if ( preview ) {
				surfaceShader = sp;
				break;
			}
		}
	} else {
		preview = freeusd::usdShade::PreviewSurface::ReadFromPrim( stage, surfaceShader );
	}

	if ( !preview ) {
		return;
	}

	if ( preview.GetDiffuseTextureAssetPath( &texPath, time ) && !texPath.empty() ) {
		R_Freeusd_AssetPathToShaderQpath( texPath, out, outSize );
		ri.Printf( PRINT_DEVELOPER, "FreeUSD: material %s -> shader '%s'\n",
			node->material_path.GetString().c_str(), out );
		return;
	}

	if ( preview.GetDiffuseColor( &diffuse, time ) ) {
		ri.Printf( PRINT_DEVELOPER,
			"FreeUSD: PreviewSurface diffuse (%.3f,%.3f,%.3f) on %s — using '%s' (no diffuse texture)\n",
			diffuse.x(), diffuse.y(), diffuse.z(), node->material_path.GetString().c_str(), out );
	}
}

static qboolean R_Freeusd_LoadMeshPrim( std::shared_ptr<freeusd::usd::Stage> stage,
	const freeusd::usdUtils::EngineSceneSnapshot &snap,
	const freeusd::sdf::Path &meshPath, double time,
	float **outVerts, int *outNumVerts, int **outInds, int *outNumIdx, float **outVertSt ) {
	freeusd::usdGeom::Mesh mesh( stage->GetPrimAtPath( meshPath ) );
	std::vector<freeusd::gf::Vec3f> points;
	std::vector<int> faceCounts;
	std::vector<int> faceIndices;
	std::vector<freeusd::usdGeom::TexCoord2f> primSt;
	const freeusd::usdUtils::EngineSceneNode *node;
	float *verts = nullptr;
	float *vertSt = nullptr;
	int *inds = nullptr;
	qboolean haveSt = qfalse;
	int numVerts = 0;
	int numTris = 0;
	int maxTris = 0;
	int faceBase = 0;
	int f;

	*outVerts = nullptr;
	*outNumVerts = 0;
	*outInds = nullptr;
	*outNumIdx = 0;
	if ( outVertSt ) {
		*outVertSt = nullptr;
	}

	if ( !mesh ) {
		return qfalse;
	}

	points = mesh.GetPoints( time );
	faceCounts = mesh.GetFaceVertexCounts( time );
	faceIndices = mesh.GetFaceVertexIndices( time );
	primSt = mesh.GetPrimvarsSt( time );
	if ( points.empty() || faceCounts.empty() || faceIndices.empty() ) {
		ri.Printf( PRINT_WARNING, "FreeUSD mesh '%s': no tessellatable geometry\n",
			meshPath.GetString().c_str() );
		return qfalse;
	}

	for ( int c : faceCounts ) {
		if ( c >= 3 ) {
			maxTris += c - 2;
		}
	}
	if ( maxTris <= 0 || maxTris * 3 > SHADER_MAX_INDEXES ) {
		return qfalse;
	}

	numVerts = maxTris * 3;
	haveSt = ( primSt.size() == points.size() );
	verts = (float *)ri.Malloc( (size_t)numVerts * 3 * sizeof( *verts ) );
	inds = (int *)ri.Malloc( (size_t)numVerts * sizeof( *inds ) );
	if ( haveSt && outVertSt ) {
		vertSt = (float *)ri.Malloc( (size_t)numVerts * 2 * sizeof( *vertSt ) );
	}
	if ( !verts || !inds || ( haveSt && outVertSt && !vertSt ) ) {
		if ( verts ) {
			ri.Free( verts );
		}
		if ( inds ) {
			ri.Free( inds );
		}
		if ( vertSt ) {
			ri.Free( vertSt );
		}
		return qfalse;
	}

	node = R_Freeusd_FindNode( snap, meshPath );
	faceBase = 0;
	for ( f = 0; f < (int)faceCounts.size(); f++ ) {
		const int nv = faceCounts[f];
		int i;

		if ( nv < 3 ) {
			faceBase += nv;
			continue;
		}
		for ( i = 1; i < nv - 1; i++ ) {
			const int corner[3] = {
				faceIndices[faceBase + 0],
				faceIndices[faceBase + i],
				faceIndices[faceBase + i + 1]
			};
			int c;
			qboolean badCorner = qfalse;

			for ( c = 0; c < 3; c++ ) {
				const int pi = corner[c];
				if ( pi < 0 || pi >= (int)points.size() ) {
					badCorner = qtrue;
					break;
				}
			}
			if ( badCorner ) {
				continue;
			}

			for ( c = 0; c < 3; c++ ) {
				const int pi = corner[c];
				float wx, wy, wz;
				const int vo = numTris * 3 + c;

				if ( node ) {
					R_Freeusd_TransformPoint( node->local_to_world_transform,
						points[pi].x(), points[pi].y(), points[pi].z(), &wx, &wy, &wz );
				} else {
					wx = points[pi].x();
					wy = points[pi].y();
					wz = points[pi].z();
				}
				verts[vo * 3 + 0] = wx;
				verts[vo * 3 + 1] = wy;
				verts[vo * 3 + 2] = wz;
				inds[vo] = vo;
				if ( haveSt && vertSt ) {
					vertSt[vo * 2 + 0] = primSt[pi].s;
					vertSt[vo * 2 + 1] = primSt[pi].t;
				}
			}
			numTris++;
		}
		faceBase += nv;
	}

	if ( numTris < 1 ) {
		ri.Free( verts );
		ri.Free( inds );
		if ( vertSt ) {
			ri.Free( vertSt );
		}
		return qfalse;
	}
	numVerts = numTris * 3;

	ri.Printf( PRINT_DEVELOPER, "FreeUSD: tessellated mesh %s (%d tris%s)\n",
		meshPath.GetString().c_str(), numTris, haveSt ? ", ST" : "" );

	*outVerts = verts;
	*outNumVerts = numVerts;
	*outInds = inds;
	*outNumIdx = numVerts;
	if ( outVertSt ) {
		*outVertSt = vertSt;
	} else if ( vertSt ) {
		ri.Free( vertSt );
	}
	return qtrue;
}

}  // namespace

extern "C" qboolean R_Freeusd_BuildMeshBuffers( const char *qpath, float **verts, int *numVerts,
	int **inds, int *numIdx, float **vertSt, char *shaderNameOut, int shaderNameOutSize ) {
	std::string osPath = R_Freeusd_BuildOSPath( qpath );
	std::string err;
	std::shared_ptr<freeusd::usd::Stage> stage;
	freeusd::usdUtils::EngineSceneSnapshot snap;
	freeusd::sdf::Path chosen;

	if ( osPath.empty() ) {
		return qfalse;
	}

	stage = freeusd::usd::Stage::OpenFromRootFile( osPath, freeusd::usd::RootLayerSublayersPolicy::DepthFirst, &err );
	if ( !stage ) {
		ri.Printf( PRINT_WARNING, "FreeUSD: failed to open '%s': %s\n", qpath, err.c_str() );
		return qfalse;
	}

	{
		const double time = R_Freeusd_TimeCode();

		snap = freeusd::usdUtils::BuildEngineSceneSnapshot( *stage, time );
		chosen = R_Freeusd_ChooseMeshPath( stage, snap, time );
	}

	if ( chosen.IsEmpty() ) {
		ri.Printf( PRINT_WARNING, "FreeUSD: no Mesh prims in '%s'\n", qpath );
		return qfalse;
	}

	if ( shaderNameOut && shaderNameOutSize > 0 ) {
		const freeusd::usdUtils::EngineSceneNode *node = R_Freeusd_FindNode( snap, chosen );
		R_Freeusd_ResolveShaderForMesh( stage, node, R_Freeusd_TimeCode(), shaderNameOut,
			(size_t)shaderNameOutSize );
	}

	return R_Freeusd_LoadMeshPrim( stage, snap, chosen, R_Freeusd_TimeCode(), verts, numVerts, inds,
		numIdx, vertSt );
}

#endif /* USE_FREEUSD */
