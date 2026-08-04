/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

USD / USDA mesh tessellation via FreeUSD (C++ only; no tr_local.h).
===========================================================================
*/

#include <cstdlib>
#include <cmath>
#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <vector>

extern "C" {
#include "q_shared.h"
#include "qfiles.h"
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

static void R_Freeusd_TransformNormal( const freeusd::gf::Matrix4d &m, float lx, float ly, float lz,
	float *ox, float *oy, float *oz ) {
	float length;
	*ox = lx * (float)m.m[0] + ly * (float)m.m[4] + lz * (float)m.m[8];
	*oy = lx * (float)m.m[1] + ly * (float)m.m[5] + lz * (float)m.m[9];
	*oz = lx * (float)m.m[2] + ly * (float)m.m[6] + lz * (float)m.m[10];
	length = sqrtf( *ox * *ox + *oy * *oy + *oz * *oz );
	if ( length > 0.00001f ) {
		*ox /= length;
		*oy /= length;
		*oz /= length;
	}
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
	const std::span<const int> faces( faceCounts );
	int tris = 0;
	for ( int c : faces ) {
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
	auto inspectPath = [&]( const freeusd::sdf::Path &path ) {
		freeusd::usdGeom::Mesh mesh( stage->GetPrimAtPath( path ) );
		int numTris;

		if ( !mesh || mesh.GetPoints( time ).empty() ) {
			return;
		}
		numTris = R_Freeusd_CountMeshTris( mesh, time );
		if ( numTris <= 0 ) {
			return;
		}
		if ( pathFilter && pathFilter[0] ) {
			if ( path.GetString().find( pathFilter ) == std::string::npos ) {
				return;
			}
		}
		out.push_back( { path, numTris } );
	};

	/* primOrder is optional USDA metadata.  Most exporters, including the
	 * Sponza layer, provide the composed prim tree without authoring it. */
	if ( !snap.prim_order.empty() ) {
		for ( const auto &path : snap.prim_order ) {
			inspectPath( path );
		}
	} else {
		for ( const auto &node : snap.nodes ) {
			inspectPath( node.path );
		}
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

static void R_Freeusd_AssetPathToShaderQpath( const std::string &asset, const char *usdQpath,
	char *out, size_t outSize ) {
	std::string p = asset;
	size_t dot;
	size_t slash;

	while ( !p.empty() && ( p.front() == '@' || p.front() == ' ' ) ) {
		p.erase( p.begin() );
	}
	while ( !p.empty() && ( p.back() == '@' || p.back() == ' ' ) ) {
		p.pop_back();
	}
	if ( p.size() >= 2 && p[0] == '.' && p[1] == '/' ) {
		p = p.substr( 2 );
	}
	for ( char &c : p ) {
		if ( c == '\\' ) c = '/';
	}
	if ( usdQpath && p.rfind( "textures/", 0 ) == 0 ) {
		std::string base = usdQpath;
		slash = base.find_last_of( '/' );
		if ( slash != std::string::npos ) {
			p = base.substr( 0, slash + 1 ) + p;
		}
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

struct FreeusdAlphaPolicy {
	qboolean hasOpacity = qfalse;
	qboolean hasOpacityThreshold = qfalse;
	float opacity = 1.0f;
	float opacityThreshold = 0.0f;
};

static void R_Freeusd_ResolveShaderForMesh( std::shared_ptr<freeusd::usd::Stage> stage,
	const freeusd::usdUtils::EngineSceneNode *node, const freeusd::sdf::Path &meshPath,
	const char *usdQpath, double time,
	char *out, size_t outSize, FreeusdAlphaPolicy *alphaOut ) {
	std::string texPath;
	freeusd::usdShade::Material material;
	freeusd::usdShade::PreviewSurface preview;
	freeusd::gf::Vec3f diffuse;
	freeusd::sdf::Path surfaceShader;
	freeusd::sdf::Path materialPath;
	freeusd::tf::Token binding( "material:binding" );

	if ( !out || outSize < 2 ) {
		return;
	}
	if ( alphaOut ) *alphaOut = FreeusdAlphaPolicy{};
	/* Default matches idtech3_demo.pk3 (demo_bootstrap.shader "white"); generic bases often use textures/common/white via mesh-import fallback when unbound. */
	Q_strncpyz( out, "white", (int)outSize );

	if ( !ri.Cvar_VariableIntegerValue( "r_freeusdShaderMap" ) ) {
		return;
	}
	/* Prefer the snapshot binding, but verify the authored prim as well.  Some
	 * USDA layers put material:binding on the mesh, while others put it on a
	 * GeomSubset or an enclosing Xform.  Keeping this fallback here makes the
	 * renderer independent of which optional EngineScene metadata survived
	 * composition. */
	if ( node && meshPath == node->path && node->has_material_binding && !node->material_path.IsEmpty() ) {
		materialPath = node->material_path;
	}
	if ( materialPath.IsEmpty() ) {
		freeusd::sdf::Path probe = meshPath;
		for ( int depth = 0; depth < 8 && !probe.IsEmpty(); depth++ ) {
			freeusd::usd::Prim prim = stage->GetPrimAtPath( probe );
			if ( prim.IsValid() ) {
				const auto targets = prim.GetRelationshipTargets( binding );
				if ( !targets.empty() ) {
					materialPath = targets.front();
					break;
				}
				if ( probe == meshPath ) {
					const auto subsets = prim.GetChildren();
					for ( const auto &subset : subsets ) {
						const auto subsetTargets = subset.GetRelationshipTargets( binding );
						if ( !subsetTargets.empty() ) {
							materialPath = subsetTargets.front();
							break;
						}
					}
				}
			}
			if ( !materialPath.IsEmpty() ) {
				break;
			}
			probe = probe.GetParentPath();
		}
	}
	if ( !materialPath.IsEmpty() ) {
		material = freeusd::usdShade::Material::ReadFromPrim( stage, materialPath );
	}
	if ( !material ) {
		if ( ri.Cvar_VariableIntegerValue( "developer" ) ) {
			ri.Printf( PRINT_DEVELOPER, "FreeUSD: no material resolved for %s (node=%s)\n",
				meshPath.GetString().c_str(), node ? "present" : "missing" );
		}
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
		if ( ri.Cvar_VariableIntegerValue( "developer" ) ) {
			ri.Printf( PRINT_DEVELOPER, "FreeUSD: material has no PreviewSurface for %s\n",
				node && !node->material_path.IsEmpty() ? node->material_path.GetString().c_str() : meshPath.GetString().c_str() );
		}
		return;
	}
	if ( alphaOut ) {
		alphaOut->hasOpacity = preview.GetOpacity( &alphaOut->opacity, time ) ? qtrue : qfalse;
		alphaOut->hasOpacityThreshold = preview.GetOpacityThreshold( &alphaOut->opacityThreshold, time ) ? qtrue : qfalse;
	}

	/* Keep the source material's alpha policy visible at the renderer seam.
	 * The eventual per-subset surface builder will consume these values to
	 * select alpha-test versus WBOIT; logging them here also makes a USDA
	 * import auditable instead of silently treating every surface as opaque. */
	if ( ri.Cvar_VariableIntegerValue( "developer" ) ) {
		float opacity = 1.0f;
		float opacityThreshold = 0.5f;
		const qboolean hasOpacity = preview.GetOpacity( &opacity, time ) ? qtrue : qfalse;
		const qboolean hasOpacityThreshold = preview.GetOpacityThreshold( &opacityThreshold, time ) ? qtrue : qfalse;
		ri.Printf( PRINT_DEVELOPER,
			"FreeUSD: material alpha policy %s opacity=%.3f%s threshold=%.3f%s (blend=%s, alpha-test=%s)\n",
			materialPath.GetString().c_str(), opacity,
			 hasOpacity ? "" : " default",
			opacityThreshold, hasOpacityThreshold ? "" : " default",
			( hasOpacity && opacity < 0.999f ) ? "candidate" : "no",
			( hasOpacityThreshold && opacityThreshold > 0.0f ) ? "candidate" : "no" );
	}

	if ( preview.GetDiffuseTextureAssetPath( &texPath, time ) && !texPath.empty() ) {
		R_Freeusd_AssetPathToShaderQpath( texPath, usdQpath, out, outSize );
		ri.Printf( PRINT_DEVELOPER, "FreeUSD: material %s -> shader '%s'\n",
			materialPath.GetString().c_str(), out );
		return;
	}

	if ( preview.GetDiffuseColor( &diffuse, time ) ) {
		ri.Printf( PRINT_DEVELOPER,
			"FreeUSD: PreviewSurface diffuse (%.3f,%.3f,%.3f) on %s — using '%s' (no diffuse texture)\n",
			diffuse.x(), diffuse.y(), diffuse.z(), materialPath.GetString().c_str(), out );
	}
}

struct FreeusdGeomSubsetInfo {
	std::vector<int> faces;
	char shaderName[R_FREEUSD_SHADERNAME_MAX];
	FreeusdAlphaPolicy alpha;
};

static qboolean R_Freeusd_LoadMeshPrim( std::shared_ptr<freeusd::usd::Stage> stage,
	const freeusd::usdUtils::EngineSceneSnapshot &snap,
	const freeusd::sdf::Path &meshPath, const char *usdQpath, double time,
	float **outVerts, int *outNumVerts, int **outInds, int *outNumIdx,
	float **outVertSt, float **outVertNormals,
	freeusdMeshSurface_t **outSurfaces, int *outNumSurfaces ) {
	freeusd::usdGeom::Mesh mesh( stage->GetPrimAtPath( meshPath ) );
	std::vector<freeusd::gf::Vec3f> points;
	std::vector<int> faceCounts;
	std::vector<int> faceIndices;
	std::vector<freeusd::usdGeom::TexCoord2f> primSt;
	std::vector<freeusd::gf::Vec3f> normals;
	std::vector<FreeusdGeomSubsetInfo> subsets;
	std::vector<int> faceSubset;
	std::vector<int> triSubset;
	const freeusd::usdUtils::EngineSceneNode *node;
	float *verts = nullptr;
	float *vertSt = nullptr;
	float *vertNormals = nullptr;
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
	if ( outSurfaces ) *outSurfaces = nullptr;
	if ( outNumSurfaces ) *outNumSurfaces = 0;
	if ( outVertSt ) {
		*outVertSt = nullptr;
	}
	if ( outVertNormals ) {
		*outVertNormals = nullptr;
	}

	if ( !mesh ) {
		return qfalse;
	}

	points = mesh.GetPoints( time );
	faceCounts = mesh.GetFaceVertexCounts( time );
	faceIndices = mesh.GetFaceVertexIndices( time );
	primSt = mesh.GetPrimvarsSt( time );
	normals = mesh.GetNormals( time );
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
	if ( maxTris <= 0 ) {
		return qfalse;
	}
	faceSubset.assign( faceCounts.size(), -1 );
	{
		const freeusd::usd::Prim meshPrim = mesh.GetPrim();
		const freeusd::tf::Token indicesToken( "indices" );
		for ( const auto &child : meshPrim.GetChildren() ) {
			freeusd::tf::Token type;
			std::string typeText;
			const freeusd::vt::Value typeValue = child.GetAttribute( freeusd::tf::Token( "elementType" ), time );
			if ( !typeValue.GetToken( &type ) ) typeValue.GetString( &typeText );
			if ( ( !type.IsEmpty() && type.GetText() != "face" ) || ( !typeText.empty() && typeText != "face" ) ) continue;
			std::vector<std::int32_t> authoredFaces;
			if ( !child.GetAttribute( indicesToken, time ).GetInt32Array( &authoredFaces ) || authoredFaces.empty() ) continue;
			FreeusdGeomSubsetInfo info;
			Q_strncpyz( info.shaderName, "white", sizeof( info.shaderName ) );
			const freeusd::usdUtils::EngineSceneNode *subsetNode = R_Freeusd_FindNode( snap, meshPath );
			R_Freeusd_ResolveShaderForMesh( stage, subsetNode, child.GetPath(), usdQpath, time,
				info.shaderName, sizeof( info.shaderName ), &info.alpha );
			for ( const std::int32_t face : authoredFaces ) {
				if ( face >= 0 && face < (std::int32_t)faceSubset.size() && faceSubset[(size_t)face] < 0 ) {
					faceSubset[(size_t)face] = (int)subsets.size();
					info.faces.push_back( face );
				}
			}
			if ( !info.faces.empty() ) subsets.push_back( info );
		}
	}

	numVerts = maxTris * 3;
	haveSt = ( primSt.size() == points.size() );
	verts = (float *)ri.Malloc( (size_t)numVerts * 3 * sizeof( *verts ) );
	inds = (int *)ri.Malloc( (size_t)numVerts * sizeof( *inds ) );
	if ( haveSt && outVertSt ) {
		vertSt = (float *)ri.Malloc( (size_t)numVerts * 2 * sizeof( *vertSt ) );
	}
	if ( normals.size() == points.size() && outVertNormals ) {
		vertNormals = (float *)ri.Malloc( (size_t)numVerts * 3 * sizeof( *vertNormals ) );
	}
	if ( !verts || !inds || ( haveSt && outVertSt && !vertSt ) ||
		( normals.size() == points.size() && outVertNormals && !vertNormals ) ) {
		if ( verts ) {
			ri.Free( verts );
		}
		if ( inds ) {
			ri.Free( inds );
		}
		if ( vertSt ) {
			ri.Free( vertSt );
		}
		if ( vertNormals ) {
			ri.Free( vertNormals );
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
				if ( vertNormals ) {
					R_Freeusd_TransformNormal( node ? node->local_to_world_transform : freeusd::gf::Matrix4d{},
						normals[pi].x(), normals[pi].y(), normals[pi].z(),
						&vertNormals[vo * 3 + 0], &vertNormals[vo * 3 + 1], &vertNormals[vo * 3 + 2] );
				}
			}
		numTris++;
			triSubset.push_back( f < (int)faceSubset.size() ? faceSubset[(size_t)f] : -1 );
		}
		faceBase += nv;
	}

	if ( numTris < 1 ) {
		ri.Free( verts );
		ri.Free( inds );
		if ( vertSt ) {
			ri.Free( vertSt );
		}
		if ( vertNormals ) {
			ri.Free( vertNormals );
		}
		return qfalse;
	}
	numVerts = numTris * 3;

	/* Group triangle-expanded vertices by authored material. MD3 has no
	 * per-triangle material index, so grouping is the point where GeomSubset
	 * ownership becomes real render surfaces. */
	if ( !subsets.empty() && (int)triSubset.size() == numTris ) {
		std::vector<int> order;
		std::vector<int> counts( subsets.size(), 0 );
		int unassigned = 0;
		for ( int ti = 0; ti < numTris; ti++ ) {
			const int si = triSubset[(size_t)ti];
			if ( si >= 0 && si < (int)subsets.size() ) counts[(size_t)si]++;
			else unassigned++;
		}
		for ( size_t si = 0; si < subsets.size(); si++ ) for ( int ti = 0; ti < numTris; ti++ ) if ( triSubset[(size_t)ti] == (int)si ) order.push_back( ti );
		for ( int ti = 0; ti < numTris; ti++ ) if ( triSubset[(size_t)ti] < 0 ) order.push_back( ti );
		std::vector<float> groupedVerts( (size_t)numVerts * 3u ), groupedSt, groupedNormals;
		if ( vertSt ) groupedSt.resize( (size_t)numVerts * 2u );
		if ( vertNormals ) groupedNormals.resize( (size_t)numVerts * 3u );
		for ( int newTri = 0; newTri < numTris; newTri++ ) {
			const int oldTri = order[(size_t)newTri];
			Com_Memcpy( groupedVerts.data() + (size_t)newTri * 9u, verts + (size_t)oldTri * 9u, 9u * sizeof( float ) );
			if ( vertSt ) Com_Memcpy( groupedSt.data() + (size_t)newTri * 6u, vertSt + (size_t)oldTri * 6u, 6u * sizeof( float ) );
			if ( vertNormals ) Com_Memcpy( groupedNormals.data() + (size_t)newTri * 9u, vertNormals + (size_t)oldTri * 9u, 9u * sizeof( float ) );
		}
		Com_Memcpy( verts, groupedVerts.data(), groupedVerts.size() * sizeof( float ) );
		if ( vertSt ) Com_Memcpy( vertSt, groupedSt.data(), groupedSt.size() * sizeof( float ) );
		if ( vertNormals ) Com_Memcpy( vertNormals, groupedNormals.data(), groupedNormals.size() * sizeof( float ) );
		if ( outSurfaces ) {
			const int outputCount = (int)subsets.size() + ( unassigned ? 1 : 0 );
			freeusdMeshSurface_t *surfaceOut = (freeusdMeshSurface_t *)ri.Malloc( (size_t)outputCount * sizeof( *surfaceOut ) );
			int firstTri = 0, outIndex = 0;
			if ( surfaceOut ) {
				for ( size_t si = 0; si < subsets.size(); si++ ) if ( counts[si] > 0 ) {
					surfaceOut[outIndex].firstTri = firstTri; surfaceOut[outIndex].numTris = counts[si];
					Q_strncpyz( surfaceOut[outIndex].shaderName, subsets[si].shaderName, sizeof( surfaceOut[outIndex].shaderName ) );
					surfaceOut[outIndex].hasOpacity = subsets[si].alpha.hasOpacity;
					surfaceOut[outIndex].hasOpacityThreshold = subsets[si].alpha.hasOpacityThreshold;
					surfaceOut[outIndex].opacity = subsets[si].alpha.opacity;
					surfaceOut[outIndex].opacityThreshold = subsets[si].alpha.opacityThreshold;
					firstTri += counts[si]; outIndex++;
				}
				if ( unassigned ) { surfaceOut[outIndex].firstTri = firstTri; surfaceOut[outIndex].numTris = unassigned; Q_strncpyz( surfaceOut[outIndex].shaderName, "white", sizeof( surfaceOut[outIndex].shaderName ) ); outIndex++; }
				*outSurfaces = surfaceOut; if ( outNumSurfaces ) *outNumSurfaces = outIndex;
			}
		}
	}

	/* Fan triangulation may skip bad corners; shrink over-allocated buffers. */
	if ( numVerts < maxTris * 3 ) {
		float *tightVerts = (float *)ri.Malloc( (size_t)numVerts * 3 * sizeof( *tightVerts ) );
		int *tightInds = (int *)ri.Malloc( (size_t)numVerts * sizeof( *tightInds ) );
		float *tightSt = nullptr;
		if ( !tightVerts || !tightInds ) {
			if ( tightVerts ) {
				ri.Free( tightVerts );
			}
			if ( tightInds ) {
				ri.Free( tightInds );
			}
			ri.Free( verts );
			ri.Free( inds );
			if ( vertSt ) {
				ri.Free( vertSt );
			}
			if ( vertNormals ) {
				ri.Free( vertNormals );
			}
			return qfalse;
		}
		Com_Memcpy( tightVerts, verts, (size_t)numVerts * 3 * sizeof( *tightVerts ) );
		Com_Memcpy( tightInds, inds, (size_t)numVerts * sizeof( *tightInds ) );
		ri.Free( verts );
		ri.Free( inds );
		verts = tightVerts;
		inds = tightInds;
		if ( vertSt ) {
			tightSt = (float *)ri.Malloc( (size_t)numVerts * 2 * sizeof( *tightSt ) );
			if ( !tightSt ) {
				ri.Free( verts );
				ri.Free( inds );
				ri.Free( vertSt );
				return qfalse;
			}
			Com_Memcpy( tightSt, vertSt, (size_t)numVerts * 2 * sizeof( *tightSt ) );
			ri.Free( vertSt );
			vertSt = tightSt;
		}
		if ( vertNormals ) {
			float *tightNormals = (float *)ri.Malloc( (size_t)numVerts * 3 * sizeof( *tightNormals ) );
			if ( !tightNormals ) {
				ri.Free( verts ); ri.Free( inds );
				if ( vertSt ) ri.Free( vertSt );
				ri.Free( vertNormals );
				return qfalse;
			}
			Com_Memcpy( tightNormals, vertNormals, (size_t)numVerts * 3 * sizeof( *tightNormals ) );
			ri.Free( vertNormals );
			vertNormals = tightNormals;
		}
	}

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
	if ( outVertNormals ) {
		*outVertNormals = vertNormals;
	} else if ( vertNormals ) {
		ri.Free( vertNormals );
	}
	return qtrue;
}

static qboolean R_Freeusd_LoadMeshSet( std::shared_ptr<freeusd::usd::Stage> stage,
	const freeusd::usdUtils::EngineSceneSnapshot &snap,
	const std::vector<FreeusdMeshCandidate> &candidates, const char *usdQpath, int triangleBudget, double time,
	float **outVerts, int *outNumVerts, int **outInds, int *outNumIdx,
	float **outVertSt, float **outVertNormals,
	freeusdMeshSurface_t **outSurfaces, int *outNumSurfaces ) {
	std::vector<float> allVerts;
	std::vector<int> allInds;
	std::vector<float> allSt;
	std::vector<float> allNormals;
	int acceptedTris = 0;
	qboolean allHaveSt = qtrue;
	qboolean allHaveNormals = qtrue;
	*outVerts = nullptr;
	*outInds = nullptr;
	*outVertSt = nullptr;
	*outVertNormals = nullptr;
	*outNumVerts = 0;
	*outNumIdx = 0;
	if ( outSurfaces ) *outSurfaces = nullptr;
	if ( outNumSurfaces ) *outNumSurfaces = 0;
	std::vector<freeusdMeshSurface_t> allSurfaces;

	for ( const auto &candidate : candidates ) {
		float *verts = nullptr, *st = nullptr, *normals = nullptr;
		freeusdMeshSurface_t *meshSurfaces = nullptr;
		int numMeshSurfaces = 0;
		int *inds = nullptr, numVerts = 0, numIdx = 0;
		if ( acceptedTris >= triangleBudget ||
			acceptedTris + candidate.numTris > triangleBudget ) {
			break;
		}
		if ( !R_Freeusd_LoadMeshPrim( stage, snap, candidate.path, usdQpath, time, &verts, &numVerts,
			&inds, &numIdx, &st, &normals, &meshSurfaces, &numMeshSurfaces ) ) {
			continue;
		}
		const int triBase = (int)allInds.size() / 3;
		allVerts.insert( allVerts.end(), verts, verts + (size_t)numVerts * 3u );
		allInds.insert( allInds.end(), inds, inds + numIdx );
		if ( st ) {
			allSt.insert( allSt.end(), st, st + (size_t)numVerts * 2u );
		} else {
			allHaveSt = qfalse;
		}
		if ( normals ) allNormals.insert( allNormals.end(), normals, normals + (size_t)numVerts * 3u );
		else allHaveNormals = qfalse;
		acceptedTris += numIdx / 3;
		for ( int si = 0; si < numMeshSurfaces; si++ ) {
			freeusdMeshSurface_t surface = meshSurfaces[si];
			surface.firstTri += triBase;
			allSurfaces.push_back( surface );
		}
		ri.Free( verts );
		ri.Free( inds );
		if ( st ) ri.Free( st );
		if ( normals ) ri.Free( normals );
		if ( meshSurfaces ) ri.Free( meshSurfaces );
	}

	if ( allVerts.empty() || allInds.empty() ) return qfalse;
	*outVerts = (float *)ri.Malloc( allVerts.size() * sizeof( float ) );
	*outInds = (int *)ri.Malloc( allInds.size() * sizeof( int ) );
	if ( !*outVerts || !*outInds ) {
		if ( *outVerts ) ri.Free( *outVerts );
		if ( *outInds ) ri.Free( *outInds );
		*outVerts = nullptr;
		*outInds = nullptr;
		return qfalse;
	}
	Com_Memcpy( *outVerts, allVerts.data(), allVerts.size() * sizeof( float ) );
	Com_Memcpy( *outInds, allInds.data(), allInds.size() * sizeof( int ) );
	if ( allHaveSt && allSt.size() == allVerts.size() / 3u * 2u ) {
		*outVertSt = (float *)ri.Malloc( allSt.size() * sizeof( float ) );
		if ( *outVertSt ) Com_Memcpy( *outVertSt, allSt.data(), allSt.size() * sizeof( float ) );
	}
	if ( allHaveNormals && allNormals.size() == allVerts.size() ) {
		*outVertNormals = (float *)ri.Malloc( allNormals.size() * sizeof( float ) );
		if ( *outVertNormals ) Com_Memcpy( *outVertNormals, allNormals.data(), allNormals.size() * sizeof( float ) );
	}
	if ( outSurfaces && !allSurfaces.empty() ) {
		*outSurfaces = (freeusdMeshSurface_t *)ri.Malloc( allSurfaces.size() * sizeof( freeusdMeshSurface_t ) );
		if ( *outSurfaces ) {
			Com_Memcpy( *outSurfaces, allSurfaces.data(), allSurfaces.size() * sizeof( freeusdMeshSurface_t ) );
			if ( outNumSurfaces ) *outNumSurfaces = (int)allSurfaces.size();
		}
	}
	*outNumVerts = (int)( allVerts.size() / 3u );
	*outNumIdx = (int)allInds.size();
	ri.Printf( PRINT_DEVELOPER, "FreeUSD: composed %zu meshes (%d tris, budget %d)\n",
		candidates.size(), acceptedTris, triangleBudget );
	return qtrue;
}

}  // namespace

extern "C" qboolean R_Freeusd_BuildMeshBuffers( const char *qpath, float **verts, int *numVerts,
	int **inds, int *numIdx, float **vertSt, float **vertNormals,
	char *shaderNameOut, int shaderNameOutSize,
	freeusdMeshSurface_t **surfacesOut, int *numSurfacesOut ) {
	std::string osPath = R_Freeusd_BuildOSPath( qpath );
	std::string err;
	std::shared_ptr<freeusd::usd::Stage> stage;
	freeusd::usdUtils::EngineSceneSnapshot snap;
	freeusd::sdf::Path chosen;
	const double diagnosticTime = R_Freeusd_TimeCode();

	if ( osPath.empty() ) {
		return qfalse;
	}

	stage = freeusd::usd::Stage::OpenFromRootFile( osPath, freeusd::usd::RootLayerSublayersPolicy::DepthFirst, &err );
	if ( !stage ) {
		ri.Printf( PRINT_WARNING, "FreeUSD: failed to open '%s': %s\n", qpath, err.c_str() );
		return qfalse;
	}

	snap = freeusd::usdUtils::BuildEngineSceneSnapshot( *stage, diagnosticTime );
	const std::vector<FreeusdMeshCandidate> candidates = R_Freeusd_ListMeshCandidates( stage, snap, diagnosticTime );
	const qboolean importAll = ri.Cvar_VariableIntegerValue( "r_freeusdImportAllMeshes" ) ? qtrue : qfalse;
	if ( importAll && !candidates.empty() ) {
		chosen = candidates.front().path;
	} else {
		chosen = R_Freeusd_ChooseMeshPath( stage, snap, diagnosticTime );
	}

	if ( chosen.IsEmpty() ) {
		ri.Printf( PRINT_WARNING, "FreeUSD: no Mesh prims in '%s' (primOrder=%zu nodes=%zu)\n",
			qpath, snap.prim_order.size(), snap.nodes.size() );
		if ( ri.Cvar_VariableIntegerValue( "developer" ) ) {
			int inspected = 0;
			for ( const auto &path : snap.prim_order ) {
				freeusd::usdGeom::Mesh mesh( stage->GetPrimAtPath( path ) );
				if ( inspected++ >= 8 ) {
					break;
				}
				ri.Printf( PRINT_WARNING, "FreeUSD: candidate[%d] %s valid=%d points=%zu faces=%zu\n",
					inspected - 1, path.GetString().c_str(), mesh ? 1 : 0,
					mesh ? mesh.GetPoints( diagnosticTime ).size() : 0u,
					mesh ? mesh.GetFaceVertexCounts( diagnosticTime ).size() : 0u );
			}
		}
		return qfalse;
	}

	if ( shaderNameOut && shaderNameOutSize > 0 ) {
		const freeusd::usdUtils::EngineSceneNode *node = R_Freeusd_FindNode( snap, chosen );
		R_Freeusd_ResolveShaderForMesh( stage, node, chosen, qpath, R_Freeusd_TimeCode(), shaderNameOut,
			(size_t)shaderNameOutSize, nullptr );
	}
	if ( surfacesOut ) *surfacesOut = nullptr;
	if ( numSurfacesOut ) *numSurfacesOut = 0;
	if ( vertNormals ) *vertNormals = nullptr;

	qboolean loaded;
	if ( importAll ) {
		loaded = R_Freeusd_LoadMeshSet( stage, snap, candidates,
			qpath, ri.Cvar_VariableIntegerValue( "r_freeusdMeshBudget" ), diagnosticTime,
			verts, numVerts, inds, numIdx, vertSt, vertNormals, surfacesOut, numSurfacesOut );
	} else {
		loaded = R_Freeusd_LoadMeshPrim( stage, snap, chosen, qpath, diagnosticTime,
			verts, numVerts, inds, numIdx, vertSt, vertNormals, surfacesOut, numSurfacesOut );
	}
	if ( !loaded ) {
		ri.Printf( PRINT_WARNING, "FreeUSD: mesh buffer build failed for '%s'\n",
			chosen.GetString().c_str() );
	}
	return loaded;
}

#endif /* USE_FREEUSD */
