/*
===========================================================================

Assimp-backed model loader for Vulkan renderer

Supports static mesh import (OBJ, etc.) via Assimp, merging all meshes into
one MD3-like surface. Animated bones/skins are not yet handled.

===========================================================================
*/

#include "tr_local.h"

#ifdef USE_ASSIMP

// Assimp headers
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

static inline void Assimp_EncodeLatLng( const aiVector3D &n, short *outNormal ) {
	float lat, lng;
	if ( n.x == 0.0f && n.y == 0.0f ) {
		lat = 0.0f;
		lng = 0.0f;
	} else {
		lat = atan2f( n.y, n.x );
		lng = acosf( Com_Clamp( -1.0f, 1.0f, n.z ) );
	}
	int ilat = (int)( lat * (32767.0f / (2.0f * (float)M_PI) ) ) & 0xFFFF;
	int ilng = (int)( lng * (32767.0f / (float)M_PI) ) & 0xFFFF;
	*outNormal = (short)( ( ilat << 8 ) | ( ilng & 0xFF ) );
}

/*
=================
R_RegisterAssimpModel

Load a model via Assimp and attach it to an existing model_t.
Returns the model handle (mod->index) on success, or 0 on failure.
=================
*/
extern "C" qhandle_t R_RegisterAssimpModel( const char *name, model_t *mod )
{
	void *buffer;
	int filesize;

	// Read file through Quake 3's virtual file system
	filesize = ri.FS_ReadFile( name, &buffer );
	if ( !buffer || filesize <= 0 ) {
		mod->type = MOD_BAD;
		return 0;
	}

	Assimp::Importer importer;

	const unsigned int flags =
		aiProcess_Triangulate |
		aiProcess_JoinIdenticalVertices |
		aiProcess_GenSmoothNormals |
		aiProcess_ImproveCacheLocality |
		aiProcess_OptimizeMeshes |
		aiProcess_SortByPType;

	// Load from memory buffer instead of file path
	const aiScene *scene = importer.ReadFileFromMemory( buffer, filesize, flags, name );
	
	// Free the file buffer
	ri.FS_FreeFile( buffer );

	if ( !scene || !scene->HasMeshes() ) {
		ri.Printf( PRINT_WARNING, "R_RegisterAssimpModel(VK): failed to load '%s': %s\n",
			name, importer.GetErrorString() );
		mod->type = MOD_BAD;
		return 0;
	}

	// Merge all meshes into a single surface
	int totalVerts = 0;
	int totalTris  = 0;
	vec3_t mins, maxs;
	for ( int i = 0; i < 3; ++i ) {
		mins[i] =  99999.0f;
		maxs[i] = -99999.0f;
	}

	for ( unsigned int m = 0; m < scene->mNumMeshes; ++m ) {
		const aiMesh *mesh = scene->mMeshes[m];
		if ( !mesh->HasPositions() || !mesh->HasFaces() )
			continue;
		totalVerts += (int)mesh->mNumVertices;
		totalTris  += (int)mesh->mNumFaces;
		for ( unsigned int v = 0; v < mesh->mNumVertices; ++v ) {
			const aiVector3D &p = mesh->mVertices[v];
			if ( p.x < mins[0] ) mins[0] = p.x;
			if ( p.y < mins[1] ) mins[1] = p.y;
			if ( p.z < mins[2] ) mins[2] = p.z;
			if ( p.x > maxs[0] ) maxs[0] = p.x;
			if ( p.y > maxs[1] ) maxs[1] = p.y;
			if ( p.z > maxs[2] ) maxs[2] = p.z;
		}
	}

	if ( totalVerts <= 0 || totalTris <= 0 ) {
		mod->type = MOD_BAD;
		return 0;
	}

	const int numFrames   = 1;
	const int numSurfaces = 1; // merged
	const int numTags     = 0;
	const int numShaders  = 1;

	const int headerSize  = (int)sizeof( md3Header_t );
	const int frameSize   = (int)sizeof( md3Frame_t ) * numFrames;
	const int tagSize     = (int)sizeof( md3Tag_t ) * numTags * numFrames;
	const int surfSize    = (int)sizeof( md3Surface_t ) +
	                        (int)sizeof( md3Shader_t ) * numShaders +
	                        (int)sizeof( md3Triangle_t ) * totalTris +
	                        (int)sizeof( md3St_t ) * totalVerts +
	                        (int)sizeof( md3XyzNormal_t ) * totalVerts;

	const int totalSize   = headerSize + frameSize + tagSize + surfSize;

	md3Header_t *hdr = (md3Header_t *)ri.Hunk_Alloc( totalSize, h_low );
	if ( !hdr ) {
		mod->type = MOD_BAD;
		return 0;
	}

	Com_Memset( hdr, 0, totalSize );

	hdr->ident       = LittleLong( MD3_IDENT );
	hdr->version     = LittleLong( MD3_VERSION );
	hdr->numFrames   = LittleLong( numFrames );
	hdr->numTags     = LittleLong( numTags );
	hdr->numSurfaces = LittleLong( numSurfaces );
	hdr->numSkins    = LittleLong( numShaders );

	hdr->ofsFrames   = LittleLong( sizeof( md3Header_t ) );
	hdr->ofsTags     = LittleLong( hdr->ofsFrames + frameSize );
	hdr->ofsSurfaces = LittleLong( hdr->ofsTags + tagSize );
	hdr->ofsEnd      = LittleLong( totalSize );

	// Frame
	md3Frame_t *frame = (md3Frame_t *)( (byte *)hdr + LittleLong( hdr->ofsFrames ) );
	Com_Memset( frame, 0, sizeof( md3Frame_t ) );
	for ( int i = 0; i < 3; ++i ) {
		frame->bounds[0][i] = mins[i];
		frame->bounds[1][i] = maxs[i];
		frame->localOrigin[i] = (mins[i] + maxs[i]) * 0.5f; // Center of bounds
	}
	// radius
	vec3_t center, corner;
	VectorCopy( frame->localOrigin, center );
	VectorSet( corner, maxs[0], maxs[1], maxs[2] );
	frame->radius = Distance( center, corner );

	// Surface
	md3Surface_t *surf = (md3Surface_t *)( (byte *)hdr + LittleLong( hdr->ofsSurfaces ) );
	Com_Memset( surf, 0, sizeof( md3Surface_t ) );

	surf->ident = MD3_IDENT;
	Q_strncpyz( surf->name, "assimp_mesh", sizeof( surf->name ) );

	surf->numFrames    = LittleLong( numFrames );
	surf->numShaders   = LittleLong( numShaders );
	surf->numVerts     = LittleLong( totalVerts );
	surf->numTriangles = LittleLong( totalTris );

	const int surfHeaderSize = (int)sizeof( md3Surface_t );
	const int surfShaderOfs  = surfHeaderSize;
	const int surfTriOfs     = surfShaderOfs + (int)sizeof( md3Shader_t ) * numShaders;
	const int surfStOfs      = surfTriOfs + (int)sizeof( md3Triangle_t ) * totalTris;
	const int surfXyzOfs     = surfStOfs + (int)sizeof( md3St_t ) * totalVerts;

	surf->ofsShaders    = LittleLong( surfShaderOfs );
	surf->ofsTriangles  = LittleLong( surfTriOfs );
	surf->ofsSt         = LittleLong( surfStOfs );
	surf->ofsXyzNormals = LittleLong( surfXyzOfs );
	surf->ofsEnd        = LittleLong( surfSize );

	// Shader placeholder
	md3Shader_t *shader = (md3Shader_t *)( (byte *)surf + surfShaderOfs );
	Com_Memset( shader, 0, sizeof( md3Shader_t ) );
	Q_strncpyz( shader->name, "white", sizeof( shader->name ) );
	shader->shaderIndex = 0;

	// Triangles, texcoords, verts
	md3Triangle_t *tris = (md3Triangle_t *)( (byte *)surf + surfTriOfs );
	md3St_t       *st   = (md3St_t *)      ( (byte *)surf + surfStOfs );
	md3XyzNormal_t *xyz = (md3XyzNormal_t *)( (byte *)surf + surfXyzOfs );

	int baseVert = 0;
	int triIdx   = 0;

	for ( unsigned int m = 0; m < scene->mNumMeshes; ++m ) {
		const aiMesh *mesh = scene->mMeshes[m];
		if ( !mesh->HasPositions() || !mesh->HasFaces() )
			continue;

		// Triangles
		for ( unsigned int f = 0; f < mesh->mNumFaces; ++f ) {
			const aiFace &face = mesh->mFaces[f];
			if ( face.mNumIndices != 3 )
				continue;
			tris[triIdx].indexes[0] = LittleLong( (int)face.mIndices[0] + baseVert );
			tris[triIdx].indexes[1] = LittleLong( (int)face.mIndices[1] + baseVert );
			tris[triIdx].indexes[2] = LittleLong( (int)face.mIndices[2] + baseVert );
			triIdx++;
		}

		// Vertices
		for ( unsigned int v = 0; v < mesh->mNumVertices; ++v ) {
			const aiVector3D &p = mesh->mVertices[v];
			const aiVector3D n  = mesh->HasNormals() ? mesh->mNormals[v] : aiVector3D( 0.0f, 0.0f, 1.0f );

			int dst = baseVert + (int)v;

			xyz[dst].xyz[0] = (short)Com_Clamp( -32768.0f, 32767.0f, (float)(int)( p.x * 64.0f ) );
			xyz[dst].xyz[1] = (short)Com_Clamp( -32768.0f, 32767.0f, (float)(int)( p.y * 64.0f ) );
			xyz[dst].xyz[2] = (short)Com_Clamp( -32768.0f, 32767.0f, (float)(int)( p.z * 64.0f ) );

			Assimp_EncodeLatLng( n, &xyz[dst].normal );

			if ( mesh->HasTextureCoords( 0 ) ) {
				const aiVector3D &tc = mesh->mTextureCoords[0][v];
				st[dst].st[0] = tc.x;
				st[dst].st[1] = 1.0f - tc.y;
			} else {
				st[dst].st[0] = 0.0f;
				st[dst].st[1] = 0.0f;
			}
		}

		baseVert += (int)mesh->mNumVertices;
	}

	mod->type      = MOD_MESH;
	mod->numLods   = 1;
	mod->md3[0]    = hdr;
	mod->dataSize += totalSize;

	ri.Printf( PRINT_DEVELOPER, "R_RegisterAssimpModel(VK): loaded '%s' - %d verts, %d tris, meshes %d, bounds (%.2f %.2f %.2f) to (%.2f %.2f %.2f), radius %.2f\n",
		name, totalVerts, triIdx, (int)scene->mNumMeshes,
		mins[0], mins[1], mins[2],
		maxs[0], maxs[1], maxs[2],
		frame->radius );

	return mod->index;
}

#else

extern "C" qhandle_t R_RegisterAssimpModel( const char *name, model_t *mod )
{
	ri.Printf( PRINT_DEVELOPER, "R_RegisterAssimpModel(VK): Assimp support not compiled in (model '%s')\n",
		name ? name : "(null)" );
	mod->type = MOD_BAD;
	return 0;
}

#endif // USE_ASSIMP
