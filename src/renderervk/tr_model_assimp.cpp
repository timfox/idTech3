/*
===========================================================================
+
+ Assimp-backed model loader for Vulkan renderer
+
+ This mirrors the OpenGL renderer's Assimp integration so that
+ RE_RegisterModel can import simple static meshes (e.g. OBJ) when using
+ the Vulkan renderer.
+
+===========================================================================
*/

#include "tr_local.h"

#ifdef USE_ASSIMP

// Assimp headers
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

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
		aiProcess_GenSmoothNormals |
		aiProcess_JoinIdenticalVertices |
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

	// For the initial integration, take only the first mesh.
	const aiMesh *mesh = scene->mMeshes[0];
	if ( !mesh->HasPositions() || !mesh->HasFaces() ) {
		ri.Printf( PRINT_WARNING, "R_RegisterAssimpModel(VK): mesh '%s' missing positions or faces\n",
			name );
		mod->type = MOD_BAD;
		return 0;
	}

	const int numVerts  = mesh->mNumVertices;
	const int numFaces  = mesh->mNumFaces;
	const int numTris   = numFaces; // after aiProcess_Triangulate

	if ( numVerts <= 0 || numTris <= 0 ) {
		mod->type = MOD_BAD;
		return 0;
	}

	// Allocate a basic MD3 header + single frame/surface.
	int i;
	vec3_t mins, maxs;

	for ( i = 0; i < 3; ++i ) {
		mins[i] =  99999.0f;
		maxs[i] = -99999.0f;
	}

	for ( unsigned int v = 0; v < mesh->mNumVertices; ++v ) {
		const aiVector3D &p = mesh->mVertices[v];
		if ( p.x < mins[0] ) mins[0] = p.x;
		if ( p.y < mins[1] ) mins[1] = p.y;
		if ( p.z < mins[2] ) mins[2] = p.z;
		if ( p.x > maxs[0] ) maxs[0] = p.x;
		if ( p.y > maxs[1] ) maxs[1] = p.y;
		if ( p.z > maxs[2] ) maxs[2] = p.z;
	}

	const int numFrames   = 1;
	const int numSurfaces = 1;
	const int numTags     = 0;
	const int numShaders  = 1;

	const int headerSize  = sizeof( md3Header_t );
	const int frameSize   = sizeof( md3Frame_t ) * numFrames;
	const int tagSize     = sizeof( md3Tag_t ) * numTags * numFrames;
	const int surfSize    = sizeof( md3Surface_t ) +
	                        sizeof( md3Shader_t ) * numShaders +
	                        sizeof( md3Triangle_t ) * numTris +
	                        sizeof( md3St_t ) * numVerts +
	                        sizeof( md3XyzNormal_t ) * numVerts;

	const int totalSize   = headerSize + frameSize + tagSize + surfSize;

	md3Header_t *hdr = (md3Header_t *)ri.Hunk_Alloc( totalSize, h_low );
	if ( !hdr ) {
		mod->type = MOD_BAD;
		return 0;
	}

	Com_Memset( hdr, 0, totalSize );

	hdr->ident      = LittleLong( MD3_IDENT );
	hdr->version    = LittleLong( MD3_VERSION );
	hdr->numFrames  = LittleLong( numFrames );
	hdr->numTags    = LittleLong( numTags );
	hdr->numSurfaces= LittleLong( numSurfaces );
	hdr->numSkins   = LittleLong( numShaders );

	hdr->ofsFrames  = LittleLong( sizeof( md3Header_t ) );
	hdr->ofsTags    = LittleLong( hdr->ofsFrames + frameSize );
	hdr->ofsSurfaces= LittleLong( hdr->ofsTags + tagSize );
	hdr->ofsEnd     = LittleLong( totalSize );

	// Frame
	md3Frame_t *frame = (md3Frame_t *)( (byte *)hdr + LittleLong( hdr->ofsFrames ) );
	Com_Memset( frame, 0, sizeof( md3Frame_t ) );
	for ( i = 0; i < 3; ++i ) {
		frame->bounds[0][i] = mins[i];
		frame->bounds[1][i] = maxs[i];
		frame->localOrigin[i] = (mins[i] + maxs[i]) * 0.5f; // Center of bounds
	}
	// Calculate radius as distance from center to furthest corner
	vec3_t center, corner;
	VectorCopy( frame->localOrigin, center );
	VectorSet( corner, maxs[0], maxs[1], maxs[2] );
	frame->radius = Distance( center, corner );

	// Surface
	md3Surface_t *surf = (md3Surface_t *)( (byte *)hdr + LittleLong( hdr->ofsSurfaces ) );
	Com_Memset( surf, 0, sizeof( md3Surface_t ) );

	surf->ident = MD3_IDENT;
	Q_strncpyz( surf->name, "assimp_mesh", sizeof( surf->name ) );

	surf->numFrames   = LittleLong( numFrames );
	surf->numShaders  = LittleLong( numShaders );
	surf->numVerts    = LittleLong( numVerts );
	surf->numTriangles= LittleLong( numTris );

	const int surfHeaderSize = sizeof( md3Surface_t );
	const int surfShaderOfs  = surfHeaderSize;
	const int surfTriOfs     = surfShaderOfs + sizeof( md3Shader_t ) * numShaders;
	const int surfStOfs      = surfTriOfs + sizeof( md3Triangle_t ) * numTris;
	const int surfXyzOfs     = surfStOfs + sizeof( md3St_t ) * numVerts;

	surf->ofsShaders    = LittleLong( surfShaderOfs );
	surf->ofsTriangles  = LittleLong( surfTriOfs );
	surf->ofsSt         = LittleLong( surfStOfs );
	surf->ofsXyzNormals = LittleLong( surfXyzOfs );
	surf->ofsEnd        = LittleLong( surfSize );

	// Shader: register a shader for the model
	// Note: We set shaderIndex to 0 (default shader) and store a shader name
	// The actual shader lookup will happen during rendering, or shader registration
	// can be deferred to when the model is processed (similar to how MD3 files work)
	md3Shader_t *shader = (md3Shader_t *)( (byte *)surf + surfShaderOfs );
	Com_Memset( shader, 0, sizeof( md3Shader_t ) );
	
	// Store shader name - use "white" as default, or model name as fallback
	Q_strncpyz( shader->name, "white", sizeof( shader->name ) );
	shader->name[sizeof( shader->name ) - 1] = '\0';
	
	// Set shader index to 0 (default shader) - this will be used if shader lookup fails
	// The renderer will use tr.defaultShader when shaderIndex is 0
	shader->shaderIndex = 0;

	// Triangles
	md3Triangle_t *tris = (md3Triangle_t *)( (byte *)surf + surfTriOfs );
	for ( int f = 0; f < numFaces; ++f ) {
		const aiFace &face = mesh->mFaces[f];
		if ( face.mNumIndices != 3 ) {
			continue;
		}
		tris[f].indexes[0] = LittleLong( face.mIndices[0] );
		tris[f].indexes[1] = LittleLong( face.mIndices[1] );
		tris[f].indexes[2] = LittleLong( face.mIndices[2] );
	}

	// Texture coordinates
	md3St_t *st = (md3St_t *)( (byte *)surf + surfStOfs );
	if ( mesh->HasTextureCoords( 0 ) ) {
		for ( int v = 0; v < numVerts; ++v ) {
			const aiVector3D &tc = mesh->mTextureCoords[0][v];
			st[v].st[0] = tc.x;
			st[v].st[1] = 1.0f - tc.y;
		}
	} else {
		for ( int v = 0; v < numVerts; ++v ) {
			st[v].st[0] = 0.0f;
			st[v].st[1] = 0.0f;
		}
	}

	// Positions and normals
	md3XyzNormal_t *xyz = (md3XyzNormal_t *)( (byte *)surf + surfXyzOfs );
	for ( int v = 0; v < numVerts; ++v ) {
		const aiVector3D &p = mesh->mVertices[v];
		const aiVector3D n  = mesh->HasNormals() ? mesh->mNormals[v] : aiVector3D( 0.0f, 0.0f, 1.0f );

		xyz[v].xyz[0] = (short)Com_Clamp( -32768.0f, 32767.0f, (float)(int)( p.x * 64.0f ) );
		xyz[v].xyz[1] = (short)Com_Clamp( -32768.0f, 32767.0f, (float)(int)( p.y * 64.0f ) );
		xyz[v].xyz[2] = (short)Com_Clamp( -32768.0f, 32767.0f, (float)(int)( p.z * 64.0f ) );

		float lat, lng;
		if ( n.x == 0 && n.y == 0 ) {
			lat = 0.0f;
			lng = 0.0f;
		} else {
			lat = atan2f( n.y, n.x );
			lng = acosf( n.z );
		}
		int ilat = (int)( lat * (32767.0f / (2.0f * (float)M_PI) ) ) & 0xFFFF;
		int ilng = (int)( lng * (32767.0f / (float)M_PI) ) & 0xFFFF;
		xyz[v].normal = (short)( ( ilat << 8 ) | ( ilng & 0xFF ) );
	}

	mod->type      = MOD_MESH;
	mod->numLods   = 1;
	mod->md3[0]    = hdr;
	mod->dataSize += totalSize;

	ri.Printf( PRINT_DEVELOPER, "R_RegisterAssimpModel(VK): loaded '%s' - %d verts, %d tris, bounds (%.2f %.2f %.2f) to (%.2f %.2f %.2f), radius %.2f\n",
		name, numVerts, numTris,
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
