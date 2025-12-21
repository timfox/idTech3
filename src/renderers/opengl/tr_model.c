/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// tr_models.c -- model loading and caching

#include "tr_local.h"
#include "tr_tiki.h"

#define	LL(x) x=LittleLong(x)

static qboolean R_LoadMD3(model_t *mod, int lod, void *buffer, int fileSize, const char *name );
static qboolean R_LoadMDR(model_t *mod, void *buffer, int filesize, const char *name );

/*
====================
R_RegisterMD3
====================
*/
static qhandle_t R_RegisterMD3(const char *name, model_t *mod)
{
	union {
		uint32_t *u;
		void *v;
	} buf;
	int			lod;
	uint32_t	ident;
	qboolean	loaded = qfalse;
	int			numLoaded;
	int			fileSize;
	char filename[MAX_QPATH], namebuf[MAX_QPATH+20];
	char *fext, defex[] = "md3";

	numLoaded = 0;

	Q_strncpyz(filename, name, sizeof(filename));

	fext = strchr(filename, '.');
	if(!fext)
		fext = defex;
	else
	{
		*fext = '\0';
		fext++;
	}

	for (lod = MD3_MAX_LODS - 1 ; lod >= 0 ; lod--)
	{
		if(lod)
			Com_sprintf(namebuf, sizeof(namebuf), "%s_%d.%s", filename, lod, fext);
		else
			Com_sprintf(namebuf, sizeof(namebuf), "%s.%s", filename, fext);

		fileSize = ri.FS_ReadFile( namebuf, &buf.v );
		if ( !buf.v )
			continue;

	if ( (size_t)fileSize < sizeof( md3Header_t ) ) {
			ri.Printf( PRINT_WARNING, "%s: truncated header for %s\n", __func__, name );
			ri.FS_FreeFile( buf.v );
			break;
		}
		
		ident = LittleLong( *buf.u );
		if ( ident == MD3_IDENT ) {
			ri.Printf( PRINT_DEVELOPER, "%s: found MD3 file '%s', loading...\n", __func__, name );
			loaded = R_LoadMD3( mod, lod, buf.v, fileSize, name );
			if ( !loaded ) {
				ri.Printf( PRINT_WARNING, "%s: R_LoadMD3 failed for '%s'\n", __func__, name );
			}
		} else {
			ri.Printf( PRINT_WARNING,"%s: unknown fileid for %s (got 0x%08X, expected 0x%08X)\n", __func__, name, ident, MD3_IDENT );
		}
		
		ri.FS_FreeFile( buf.v );

		if ( loaded )
		{
			mod->numLods++;
			numLoaded++;
		}
		else
			break;
	}

	if ( numLoaded )
	{
		// duplicate into higher lod spots that weren't
		// loaded, in case the user changes r_lodbias on the fly
		for ( lod--; lod >= 0; lod-- )
		{
			mod->numLods++;
			mod->md3[lod] = mod->md3[lod + 1];
		}

		return mod->index;
	}

	ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW "%s: couldn't load %s\n", __func__, name );

	mod->type = MOD_BAD;
	return 0;
}


/*
====================
R_RegisterMDR
====================
*/
static qhandle_t R_RegisterMDR(const char *name, model_t *mod)
{
	union {
		uint32_t *u;
		void *v;
	} buf;
	uint32_t ident;
	qboolean loaded = qfalse;
	int filesize;

	filesize = ri.FS_ReadFile( name, &buf.v );
	if ( !buf.v ) {
		mod->type = MOD_BAD;
		return 0;
	}

	if ( (size_t)filesize < sizeof( ident ) ) {
		ri.FS_FreeFile( buf.v );
		mod->type = MOD_BAD;
		return 0;
	}
	
	ident = LittleLong( *buf.u );
	if ( ident == MDR_IDENT )
		loaded = R_LoadMDR( mod, buf.v, filesize, name );

	ri.FS_FreeFile( buf.v );
	
	if ( !loaded )
	{
		ri.Printf( PRINT_WARNING, "%s: couldn't load %s\n", __func__, name );
		mod->type = MOD_BAD;
		return 0;
	}
	
	return mod->index;
}


/*
====================
R_RegisterIQM
====================
*/
static qhandle_t R_RegisterIQM(const char *name, model_t *mod)
{
	union {
		unsigned *u;
		void *v;
	} buf;
	qboolean loaded = qfalse;
	int filesize;

	filesize = ri.FS_ReadFile(name, (void **) &buf.v);
	if(!buf.u)
	{
		mod->type = MOD_BAD;
		return 0;
	}
	
	loaded = R_LoadIQM(mod, buf.u, filesize, name);

	ri.FS_FreeFile (buf.v);
	
	if ( !loaded )
	{
		ri.Printf( PRINT_WARNING, "%s: couldn't load %s\n", __func__, name );
		mod->type = MOD_BAD;
		return 0;
	}
	
	return mod->index;
}


typedef struct
{
	const char *ext;
	qhandle_t (*ModelLoader)( const char *, model_t * );
} modelExtToLoaderMap_t;

// Forward declaration for TIKI loader
qhandle_t R_RegisterTIKI(const char *name, model_t *mod);
// Forward declaration for Assimp-backed loader
qhandle_t R_RegisterAssimpModel(const char *name, model_t *mod);

// Note that the ordering indicates the order of preference used
// when there are multiple models of different formats available
static modelExtToLoaderMap_t modelLoaders[ ] =
{
	{ "tiki", R_RegisterTIKI },
	{ "iqm",  R_RegisterIQM },
	{ "mdr",  R_RegisterMDR },
	{ "md3",  R_RegisterMD3 },
	// Assimp-supported formats (static meshes only)
	{ "obj",  R_RegisterAssimpModel },
	{ "dae",  R_RegisterAssimpModel }, // COLLADA
	{ "fbx",  R_RegisterAssimpModel }, // FBX
	{ "gltf", R_RegisterAssimpModel }, // glTF
	{ "glb",  R_RegisterAssimpModel }, // Binary glTF
	{ "3ds",  R_RegisterAssimpModel }, // 3DS Max
	{ "blend", R_RegisterAssimpModel }, // Blender
	{ "ply",  R_RegisterAssimpModel }, // Polygon File Format
	{ "stl",  R_RegisterAssimpModel }, // STL
	{ "x3d",  R_RegisterAssimpModel }, // X3D
	{ "ase",  R_RegisterAssimpModel }  // ASCII Scene Export
};

static int numModelLoaders = ARRAY_LEN(modelLoaders);

//===============================================================================

/*
** R_GetModelByHandle
*/
model_t	*R_GetModelByHandle( qhandle_t index ) {
	model_t		*mod;

	// out of range gets the default model
	if ( index < 1 || index >= tr.numModels ) {
		return tr.models[0];
	}

	mod = tr.models[index];

	return mod;
}

//===============================================================================

/*
** R_AllocModel
*/
model_t *R_AllocModel( void ) {
	model_t		*mod;

	if ( tr.numModels >= MAX_MOD_KNOWN ) {
		return NULL;
	}

	mod = ri.Hunk_Alloc( sizeof( *tr.models[tr.numModels] ), h_low );
	if (!mod) {
		ri.Printf(PRINT_WARNING, "R_AllocModel: Failed to allocate memory for model\n");
		return NULL;
	}
	mod->index = tr.numModels;
	tr.models[tr.numModels] = mod;
	tr.numModels++;

	return mod;
}

/*
====================
R_ModelHashKey

Generate a hash key for model name lookups
====================
*/
static unsigned int R_ModelHashKey(const char *name) {
	unsigned int hash = 0;
	int i;

	for (i = 0; name[i]; i++) {
		hash = hash * 33 + tolower(name[i]);
	}
	return hash % MODEL_HASH_SIZE;
}

/*
====================
R_AddModelToHash

Add a model to the hash table
====================
*/
static void R_AddModelToHash(model_t *mod) {
	unsigned int hash;

	if (!mod || !mod->name || !mod->name[0]) {
		return;
	}

	hash = R_ModelHashKey(mod->name);
	mod->nextHash = tr.modelHashTable[hash];
	tr.modelHashTable[hash] = mod;
}

/*
====================
R_FindModelInHash

Find a model in the hash table
====================
*/
static model_t *R_FindModelInHash(const char *name) {
	unsigned int hash;
	model_t *mod;

	hash = R_ModelHashKey(name);
	for (mod = tr.modelHashTable[hash]; mod; mod = mod->nextHash) {
		if (!Q_stricmp(mod->name, name)) {
			return mod;
		}
	}
	return NULL;
}

/*
====================
RE_RegisterModel

Loads in a model for the given name

Zero will be returned if the model fails to load.
An entry will be retained for failed models as an
optimization to prevent disk rescanning if they are
asked for again.
====================
*/
// Model loading job for async loading
typedef struct {
	char name[MAX_QPATH];
	model_t *mod;
	qboolean completed;
	qboolean success;
} modelLoadJob_t;

#define MAX_ASYNC_MODEL_JOBS 4
static modelLoadJob_t asyncModelJobs[MAX_ASYNC_MODEL_JOBS];
static int asyncModelJobCount = 0;

static void ModelLoadWorker_Async(void *data) {
	modelLoadJob_t *job = (modelLoadJob_t *)data;

	// Perform the actual model loading (this is the synchronous path)
	qhandle_t hModel = RE_RegisterModel_Sync(job->name);
	if (hModel) {
		job->success = qtrue;
		job->mod = tr.models[hModel];
	} else {
		job->success = qfalse;
	}

	job->completed = qtrue;
}

static void ModelLoadCompletion_Async(void *data) {
	modelLoadJob_t *job = (modelLoadJob_t *)data;

	if (job->success) {
		ri.Printf(PRINT_DEVELOPER, "ModelLoadCompletion_Async: Model '%s' loaded successfully\n", job->name);
	} else {
		ri.Printf(PRINT_WARNING, "ModelLoadCompletion_Async: Failed to load model '%s'\n", job->name);
	}

	// Mark job as available for reuse
	job->completed = qfalse;
	job->success = qfalse;
	asyncModelJobCount--;
}

/*
=================
RE_RegisterModel

Public interface for model registration - chooses between async and sync loading
=================
*/
qhandle_t RE_RegisterModel( const char *name ) {
#ifdef USE_JOBSYSTEM
	// Check for async loading preference
	static cvar_t *r_modelAsync = NULL;
	if (!r_modelAsync) {
		r_modelAsync = ri.Cvar_Get("r_modelAsync", "1", CVAR_ARCHIVE | CVAR_LATCH);
		ri.Cvar_SetDescription(r_modelAsync, "Use asynchronous model loading (0 = sync, 1 = async)");
	}

	if (r_modelAsync && r_modelAsync->integer) {
		return RE_RegisterModel_Async(name);
	}
#endif

	// Fall back to synchronous loading
	return RE_RegisterModel_Sync(name);
}

qhandle_t RE_RegisterModel_Async(const char *name) {
#ifdef USE_JOBSYSTEM
	// Check if we have too many concurrent async jobs
	if (asyncModelJobCount >= MAX_ASYNC_MODEL_JOBS) {
		ri.Printf(PRINT_DEVELOPER, "RE_RegisterModel_Async: Too many concurrent model loads (%d), using sync\n", asyncModelJobCount);
		return RE_RegisterModel_Sync(name);
	}

	// Find available job slot
	modelLoadJob_t *job = NULL;
	for (int i = 0; i < MAX_ASYNC_MODEL_JOBS; i++) {
		if (!asyncModelJobs[i].completed) {
			job = &asyncModelJobs[i];
			break;
		}
	}

	if (!job) {
		ri.Printf(PRINT_WARNING, "RE_RegisterModel_Async: No available job slots, using sync\n");
		return RE_RegisterModel_Sync(name);
	}

	// Initialize job
	Q_strncpyz(job->name, name, sizeof(job->name));
	job->completed = qfalse;
	job->success = qfalse;
	asyncModelJobCount++;

	// Submit job to job system
	extern job_handle_t *JobSystem_SubmitJobWithCompletion(jobFunction_t, void *, jobPriority_t, void (*)(void *), void *);
	job_handle_t *handle = JobSystem_SubmitJobWithCompletion(
		ModelLoadWorker_Async, job, JOB_PRIORITY_NORMAL,
		ModelLoadCompletion_Async, job);

	if (!handle) {
		ri.Printf(PRINT_WARNING, "RE_RegisterModel_Async: Failed to submit job, using sync\n");
		job->completed = qfalse;
		asyncModelJobCount--;
		return RE_RegisterModel_Sync(name);
	}

	ri.Printf(PRINT_DEVELOPER, "RE_RegisterModel_Async: Submitted async load for '%s'\n", name);
	return 0; // Async loading - handle will be resolved later
#else
	// Fallback to synchronous loading
	return RE_RegisterModel_Sync(name);
#endif
}

qhandle_t RE_RegisterModel_Sync( const char *name ) {
	model_t		*mod;
	qhandle_t	hModel;
	qboolean	orgNameFailed = qfalse;
	int			orgLoader = -1;
	int			i;
	char		localName[ MAX_QPATH ];
	const char	*ext;
	char		altName[ MAX_QPATH ];

	if ( !name || !name[0] ) {
		ri.Printf( PRINT_ALL, "RE_RegisterModel: NULL name\n" );
		return 0;
	}

	if ( strlen( name ) >= MAX_QPATH ) {
		ri.Printf( PRINT_ALL, "Model name exceeds MAX_QPATH\n" );
		return 0;
	}

	//
	// search the currently loaded models using hash table
	//
	mod = R_FindModelInHash(name);
	if (mod) {
		if (mod->type == MOD_BAD) {
			return 0;
		}
		return mod->index;
	}

	// allocate a new model_t

	if ( ( mod = R_AllocModel() ) == NULL ) {
		ri.Printf( PRINT_WARNING, "RE_RegisterModel: R_AllocModel() failed for '%s'\n", name);
		return 0;
	}

	// only set the name after the model has been successfully loaded
	Q_strncpyz( mod->name, name, sizeof( mod->name ) );

	//R_IssuePendingRenderCommands();

	mod->type = MOD_BAD;
	mod->numLods = 0;

	//
	// load the files
	//
	Q_strncpyz( localName, name, MAX_QPATH );

	ext = COM_GetExtension( localName );

	if( *ext )
	{
		// Look for the correct loader and use it
		for( i = 0; i < numModelLoaders; i++ )
		{
			if( !Q_stricmp( ext, modelLoaders[ i ].ext ) )
			{
				// Load
				hModel = modelLoaders[ i ].ModelLoader( localName, mod );
				break;
			}
		}

		// A loader was found
		if( i < numModelLoaders )
		{
			if( !hModel )
			{
				// Loader failed, most likely because the file isn't there;
				// try again without the extension
				orgNameFailed = qtrue;
				orgLoader = i;
				COM_StripExtension( name, localName, MAX_QPATH );
			}
			else
			{
				// Something loaded
				return mod->index;
			}
		}
	}

	// Try and find a suitable match using all
	// the model formats supported
	for( i = 0; i < numModelLoaders; i++ )
	{
		if (i == orgLoader)
			continue;

		Com_sprintf( altName, sizeof (altName), "%s.%s", localName, modelLoaders[ i ].ext );

		// Load
		hModel = modelLoaders[ i ].ModelLoader( altName, mod );

		if( hModel )
		{
			if( orgNameFailed )
			{
				ri.Printf( PRINT_DEVELOPER, "WARNING: %s not present, using %s instead\n",
						name, altName );
			}

			// Add successfully loaded model to hash table for fast lookups
			R_AddModelToHash(mod);
			break;
		}
	}

	return hModel;
}


/*
=================
R_LoadMD3
=================
*/
static qboolean R_LoadMD3( model_t *mod, int lod, void *buffer, int fileSize, const char *mod_name ) {
	int					i, j;
	md3Header_t			*pinmodel, *hdr;
	md3Frame_t			*frame;
	md3Surface_t		*surf;
	md3Shader_t			*shader;
	md3Triangle_t		*tri;
	md3St_t				*st;
	md3XyzNormal_t		*xyz;
	md3Tag_t			*tag;
	int					version;
	int					size;

	if ( !buffer ) {
		ri.Printf( PRINT_WARNING, "%s: NULL buffer for %s\n", __func__, mod_name );
		return qfalse;
	}

	// Additional security check for model files
	if ( !Q_ValidateFilePath( mod_name ) ) {
		ri.Printf( PRINT_WARNING, "R_LoadMD3: Invalid path for model: %s\n", mod_name );
		return qfalse;
	}

	pinmodel = (md3Header_t *)buffer;

	version = LittleLong( pinmodel->version );
	ri.Printf( PRINT_DEVELOPER, "%s: checking MD3 '%s' version=%i (expected %i)\n", __func__, mod_name, version, MD3_VERSION );
	if ( version != MD3_VERSION ) {
		ri.Printf( PRINT_WARNING, "%s: %s has wrong version (%i should be %i)\n", __func__, mod_name, version, MD3_VERSION );
		return qfalse;
	}

	size = LittleLong( pinmodel->ofsEnd );
	ri.Printf( PRINT_DEVELOPER, "%s: MD3 '%s' size=%i, fileSize=%i\n", __func__, mod_name, size, fileSize );

	if ( size > fileSize ) {
		ri.Printf( PRINT_WARNING, "%s: %s has corrupted header (size %i > fileSize %i)\n", __func__, mod_name, size, fileSize );
		return qfalse;
	}

	mod->type = MOD_MESH;
	mod->dataSize += size;
	mod->md3[lod] = ri.Hunk_Alloc( size, h_low );

	Com_Memcpy( mod->md3[lod], buffer, size );

	hdr = mod->md3[lod];

	LL( hdr->ident );
	LL( hdr->version );
	LL( hdr->numFrames );
	LL( hdr->numTags);
	LL( hdr->numSurfaces);
	LL( hdr->numSkins );
	LL( hdr->ofsFrames );
	LL( hdr->ofsTags );
	LL( hdr->ofsSurfaces );
	LL( hdr->ofsEnd );

	ri.Printf( PRINT_DEVELOPER, "%s: MD3 '%s' frames=%i, tags=%i, surfaces=%i, skins=%i\n",
		__func__, mod_name, hdr->numFrames, hdr->numTags, hdr->numSurfaces, hdr->numSkins );

	if ( hdr->numFrames < 1 ) {
		ri.Printf( PRINT_WARNING, "%s: %s has no frames\n", __func__, mod_name );
		return qfalse;
	}

	if ( hdr->ofsFrames > (uint32_t)size || hdr->ofsTags > (uint32_t)size || hdr->ofsSurfaces > (uint32_t)size ) {
		ri.Printf( PRINT_WARNING, "%s: %s has corrupted header\n", __func__, mod_name );
		return qfalse;
	}
	if ( (unsigned)( hdr->numFrames | hdr->numTags | hdr->numSkins ) > (1 << 20) ) {
		ri.Printf( PRINT_WARNING, "%s: %s has corrupted header\n", __func__, mod_name );
		return qfalse;
	}

	if ( hdr->ofsFrames + hdr->numFrames * sizeof( md3Frame_t ) > (size_t)fileSize ) {
		ri.Printf( PRINT_WARNING, "%s: %s has corrupted header\n", __func__, mod_name );
		return qfalse;
	}
	if ( hdr->ofsTags + hdr->numTags * hdr->numFrames * sizeof( md3Tag_t ) > (size_t)fileSize ) {
		ri.Printf( PRINT_WARNING, "%s: %s has corrupted header\n", __func__, mod_name );
		return qfalse;
	}
	if ( hdr->ofsSurfaces + ( hdr->numSurfaces ? 1 : 0 ) * sizeof( md3Surface_t ) > (size_t)fileSize ) {
		ri.Printf( PRINT_WARNING, "%s: %s has corrupted header\n", __func__, mod_name );
		return qfalse;
	}

	// swap all the frames
	frame = (md3Frame_t *) ( (byte *)hdr + hdr->ofsFrames );
	for ( i = 0 ; i < hdr->numFrames ; i++, frame++) {
		frame->radius = LittleFloat( frame->radius );
		for ( j = 0 ; j < 3 ; j++ ) {
			frame->bounds[0][j] = LittleFloat( frame->bounds[0][j] );
			frame->bounds[1][j] = LittleFloat( frame->bounds[1][j] );
			frame->localOrigin[j] = LittleFloat( frame->localOrigin[j] );
		}
	}

	// swap all the tags
	tag = (md3Tag_t *) ( (byte *)hdr + hdr->ofsTags );
	for ( i = 0 ; i < hdr->numTags * hdr->numFrames; i++, tag++ ) {
		// zero-terminate tag name
		tag->name[sizeof( tag->name ) - 1] = '\0';
		for ( j = 0 ; j < 3; j++ ) {
			tag->origin[j] = LittleFloat( tag->origin[j] );
			tag->axis[0][j] = LittleFloat( tag->axis[0][j] );
			tag->axis[1][j] = LittleFloat( tag->axis[1][j] );
			tag->axis[2][j] = LittleFloat( tag->axis[2][j] );
		}
	}

	// swap all the surfaces
	surf = (md3Surface_t *) ( (byte *)hdr + hdr->ofsSurfaces );
	for ( i = 0 ; i < hdr->numSurfaces; i++) {

		LL(surf->ident);
		LL(surf->flags);
		LL(surf->numFrames);
		LL(surf->numShaders);
		LL(surf->numTriangles);
		LL(surf->numVerts);
		LL(surf->ofsTriangles);
		LL(surf->ofsShaders);
		LL(surf->ofsSt);
		LL(surf->ofsXyzNormals);
		LL(surf->ofsEnd);

		if ( surf->ofsEnd > (size_t)fileSize || ((size_t)((byte*)surf - (byte*)hdr) + surf->ofsEnd) > (size_t)fileSize ) {
			ri.Printf( PRINT_WARNING, "%s: %s has corrupted surface header\n", __func__, mod_name );
			return qfalse;
		}
		if ( surf->ofsTriangles > (size_t)fileSize || surf->ofsShaders > (size_t)fileSize || surf->ofsSt > (size_t)fileSize || surf->ofsXyzNormals > (size_t)fileSize ) {
			ri.Printf( PRINT_WARNING, "%s: %s has corrupted surface header\n", __func__, mod_name );
			return qfalse;
		}
		if ( surf->ofsTriangles + surf->numTriangles * sizeof( md3Triangle_t ) > (size_t)fileSize ) {
			ri.Printf( PRINT_WARNING, "%s: %s has corrupted surface header\n", __func__, mod_name );
			return qfalse;
		}
		if ( surf->ofsShaders + surf->numShaders * sizeof( md3Shader_t ) > (size_t)fileSize || surf->numShaders > (1<<20) ) {
			ri.Printf( PRINT_WARNING, "%s: %s has corrupted surface header\n", __func__, mod_name );
			return qfalse;
		}
		if ( surf->ofsSt + surf->numVerts * sizeof( md3St_t ) > (size_t)fileSize ) {
			ri.Printf( PRINT_WARNING, "%s: %s has corrupted surface header\n", __func__, mod_name );
			return qfalse;
		}
		if ( surf->ofsXyzNormals + surf->numVerts * sizeof( md3XyzNormal_t ) > (size_t)fileSize ) {
			ri.Printf( PRINT_WARNING, "%s: %s has corrupted surface header\n", __func__, mod_name );
			return qfalse;
		}

		if ( surf->numVerts >= SHADER_MAX_VERTEXES ) {
			ri.Printf(PRINT_WARNING, "%s: %s has more than %i verts on %s (%i).\n", __func__,
				mod_name, SHADER_MAX_VERTEXES - 1, surf->name[0] ? surf->name : "a surface",
				surf->numVerts );
			return qfalse;
		}
		if ( surf->numTriangles*3 >= SHADER_MAX_INDEXES ) {
			ri.Printf(PRINT_WARNING, "%s: %s has more than %i triangles on %s (%i).\n", __func__,
				mod_name, ( SHADER_MAX_INDEXES / 3 ) - 1, surf->name[0] ? surf->name : "a surface",
				surf->numTriangles );
			return qfalse;
		}

		// change to surface identifier
		surf->ident = SF_MD3;

		// zero-terminate surface name
		surf->name[sizeof( surf->name ) - 1] = '\0';

		// lowercase the surface name so skin compares are faster
		Q_strlwr( surf->name );

		// strip off a trailing _1 or _2
		// this is a crutch for q3data being a mess
		j = strlen( surf->name );
		if ( j > 2 && surf->name[j-2] == '_' ) {
			surf->name[j-2] = 0;
		}

		// register the shaders
		shader = (md3Shader_t *) ( (byte *)surf + surf->ofsShaders );
		for ( j = 0 ; j < surf->numShaders ; j++, shader++ ) {
			shader_t	*sh;

			// zero-terminate shader name
			shader->name[sizeof( shader->name ) - 1] = '\0';

			sh = R_FindShader( shader->name, LIGHTMAP_NONE, qtrue );
			if ( sh->defaultShader ) {
				shader->shaderIndex = 0;
			} else {
				shader->shaderIndex = sh->index;
			}
		}

		// swap all the triangles
		tri = (md3Triangle_t *) ( (byte *)surf + surf->ofsTriangles );
		for ( j = 0 ; j < surf->numTriangles; j++, tri++ ) {
			LL(tri->indexes[0]);
			LL(tri->indexes[1]);
			LL(tri->indexes[2]);
		}

		// swap all the ST
		st = (md3St_t *) ( (byte *)surf + surf->ofsSt );
		for ( j = 0 ; j < surf->numVerts ; j++, st++ ) {
			st->st[0] = LittleFloat( st->st[0] );
			st->st[1] = LittleFloat( st->st[1] );
		}

		// swap all the XyzNormals
		xyz = (md3XyzNormal_t *) ( (byte *)surf + surf->ofsXyzNormals );
		for ( j = 0 ; j < surf->numVerts * surf->numFrames ; j++, xyz++ ) 
		{
			xyz->xyz[0] = LittleShort( xyz->xyz[0] );
			xyz->xyz[1] = LittleShort( xyz->xyz[1] );
			xyz->xyz[2] = LittleShort( xyz->xyz[2] );

			xyz->normal = LittleShort( xyz->normal );
		}

		// find the next surface
		surf = (md3Surface_t *)( (byte *)surf + surf->ofsEnd );
	}

	return qtrue;
}


/*
=================
R_LoadMDR
=================
*/
static qboolean R_LoadMDR( model_t *mod, void *buffer, int filesize, const char *mod_name ) 
{
	int					i, j, k, l;
	mdrHeader_t			*pinmodel, *mdr;
	mdrFrame_t			*frame;
	mdrLOD_t			*lod, *curlod;
	mdrSurface_t			*surf, *cursurf;
	mdrTriangle_t			*tri, *curtri;
	mdrVertex_t			*v, *curv;
	mdrWeight_t			*weight, *curweight;
	mdrTag_t			*tag, *curtag;
	int					size;
	shader_t			*sh;

	pinmodel = (mdrHeader_t *)buffer;

	pinmodel->version = LittleLong(pinmodel->version);
	if ( pinmodel->version != MDR_VERSION ) 
	{
		ri.Printf(PRINT_WARNING, "%s: %s has wrong version (%i should be %i)\n", __func__, mod_name, pinmodel->version, MDR_VERSION);
		return qfalse;
	}

	size = LittleLong(pinmodel->ofsEnd);
	
	if ( size > filesize )
	{
		ri.Printf( PRINT_WARNING, "%s: Header of %s is broken. Wrong filesize declared!\n", __func__, mod_name );
		return qfalse;
	}
	
	mod->type = MOD_MDR;

	LL(pinmodel->numFrames);
	LL(pinmodel->numBones);
	LL(pinmodel->ofsFrames);

	// This is a model that uses some type of compressed Bones. We don't want to uncompress every bone for each rendered frame
	// over and over again, we'll uncompress it in this function already, so we must adjust the size of the target mdr.
	if(pinmodel->ofsFrames < 0)
	{
		// mdrFrame_t is larger than mdrCompFrame_t:
		size += pinmodel->numFrames * sizeof(frame->name);
		// now add enough space for the uncompressed bones.
		size += pinmodel->numFrames * pinmodel->numBones * ((sizeof(mdrBone_t) - sizeof(mdrCompBone_t)));
	}
	
	// simple bounds check
	if(pinmodel->numBones < 0 ||
		sizeof(*mdr) + pinmodel->numFrames * (sizeof(*frame) + (pinmodel->numBones - 1) * sizeof(*frame->bones)) > (size_t)size)
	{
		ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has broken structure.\n", mod_name);
		return qfalse;
	}

	mod->dataSize += size;
	mod->modelData.mdr = mdr = ri.Hunk_Alloc( size, h_low );

	// Copy all the values over from the file and fix endian issues in the process, if necessary.
	
	mdr->ident = LittleLong(pinmodel->ident);
	mdr->version = pinmodel->version;	// Don't need to swap byte order on this one, we already did above.
	Q_strncpyz(mdr->name, pinmodel->name, sizeof(mdr->name));
	mdr->numFrames = pinmodel->numFrames;
	mdr->numBones = pinmodel->numBones;
	mdr->numLODs = LittleLong(pinmodel->numLODs);
	mdr->numTags = LittleLong(pinmodel->numTags);
	// We don't care about the other offset values, we'll generate them ourselves while loading.

	mod->numLods = mdr->numLODs;

	if ( mdr->numFrames < 1 ) 
	{
		ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has no frames\n", mod_name);
		return qfalse;
	}

	/* The first frame will be put into the first free space after the header */
	frame = (mdrFrame_t *)(mdr + 1);
	mdr->ofsFrames = (int)((byte *) frame - (byte *) mdr);
		
	if (pinmodel->ofsFrames < 0)
	{
		mdrCompFrame_t *cframe;
				
		// compressed model...				
		cframe = (mdrCompFrame_t *)((byte *) pinmodel - pinmodel->ofsFrames);
		
		for(i = 0; i < mdr->numFrames; i++)
		{
			for(j = 0; j < 3; j++)
			{
				frame->bounds[0][j] = LittleFloat(cframe->bounds[0][j]);
				frame->bounds[1][j] = LittleFloat(cframe->bounds[1][j]);
				frame->localOrigin[j] = LittleFloat(cframe->localOrigin[j]);
			}

			frame->radius = LittleFloat(cframe->radius);
			frame->name[0] = '\0';	// No name supplied in the compressed version.
			
			for(j = 0; j < mdr->numBones; j++)
			{
				for(k = 0; k < (int)(sizeof(cframe->bones[j].Comp) / 2); k++)
				{
					// Do swapping for the uncompressing functions. They seem to use shorts
					// values only, so I assume this will work. Never tested it on other
					// platforms, though.
					
					((unsigned short *)(cframe->bones[j].Comp))[k] =
						LittleShort( ((unsigned short *)(cframe->bones[j].Comp))[k] );
				}
				
				/* Now do the actual uncompressing */
				MC_UnCompress(frame->bones[j].matrix, cframe->bones[j].Comp);
			}
			
			// Next Frame...
			cframe = (mdrCompFrame_t *) &cframe->bones[j];
			frame = (mdrFrame_t *) &frame->bones[j];
		}
	}
	else
	{
		mdrFrame_t *curframe;
		
		// uncompressed model...
		//
    
		curframe = (mdrFrame_t *)((byte *) pinmodel + pinmodel->ofsFrames);
		
		// swap all the frames
		for ( i = 0 ; i < mdr->numFrames ; i++) 
		{
			for(j = 0; j < 3; j++)
			{
				frame->bounds[0][j] = LittleFloat(curframe->bounds[0][j]);
				frame->bounds[1][j] = LittleFloat(curframe->bounds[1][j]);
				frame->localOrigin[j] = LittleFloat(curframe->localOrigin[j]);
			}
			
			frame->radius = LittleFloat(curframe->radius);
			Q_strncpyz(frame->name, curframe->name, sizeof(frame->name));
			
			for (j = 0; j < (int) (mdr->numBones * sizeof(mdrBone_t) / 4); j++) 
			{
				((float *)frame->bones)[j] = LittleFloat( ((float *)curframe->bones)[j] );
			}
			
			curframe = (mdrFrame_t *) &curframe->bones[mdr->numBones];
			frame = (mdrFrame_t *) &frame->bones[mdr->numBones];
		}
	}
	
	// frame should now point to the first free address after all frames.
	lod = (mdrLOD_t *) frame;
	mdr->ofsLODs = (int) ((byte *) lod - (byte *)mdr);
	
	curlod = (mdrLOD_t *)((byte *) pinmodel + LittleLong(pinmodel->ofsLODs));
		
	// swap all the LOD's
	for ( l = 0 ; l < mdr->numLODs ; l++)
	{
		// simple bounds check
		if((byte *) (lod + 1) > (byte *) mdr + size)
		{
			ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has broken structure.\n", mod_name);
			return qfalse;
		}

		lod->numSurfaces = LittleLong(curlod->numSurfaces);
		
		// swap all the surfaces
		surf = (mdrSurface_t *) (lod + 1);
		lod->ofsSurfaces = (int)((byte *) surf - (byte *) lod);
		cursurf = (mdrSurface_t *) ((byte *)curlod + LittleLong(curlod->ofsSurfaces));
		
		for ( i = 0 ; i < lod->numSurfaces ; i++)
		{
			// simple bounds check
			if((byte *) (surf + 1) > (byte *) mdr + size)
			{
				ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has broken structure.\n", mod_name);
				return qfalse;
			}

			// first do some copying stuff
			
			surf->ident = SF_MDR;
			Q_strncpyz(surf->name, cursurf->name, sizeof(surf->name));
			Q_strncpyz(surf->shader, cursurf->shader, sizeof(surf->shader));
			
			surf->ofsHeader = (byte *) mdr - (byte *) surf;
			
			surf->numVerts = LittleLong(cursurf->numVerts);
			surf->numTriangles = LittleLong(cursurf->numTriangles);
			// numBoneReferences and BoneReferences generally seem to be unused
			
			// now do the checks that may fail.
			if ( surf->numVerts >= SHADER_MAX_VERTEXES ) 
			{
				ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has more than %i verts on %s (%i).\n",
					  mod_name, SHADER_MAX_VERTEXES - 1, surf->name[0] ? surf->name : "a surface",
					  surf->numVerts );
				return qfalse;
			}
			if ( surf->numTriangles*3 >= SHADER_MAX_INDEXES ) 
			{
				ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has more than %i triangles on %s (%i).\n",
					  mod_name, ( SHADER_MAX_INDEXES / 3 ) - 1, surf->name[0] ? surf->name : "a surface",
					  surf->numTriangles );
				return qfalse;
			}
			// lowercase the surface name so skin compares are faster
			Q_strlwr( surf->name );

			// register the shaders
			sh = R_FindShader(surf->shader, LIGHTMAP_NONE, qtrue);
			if ( sh->defaultShader ) {
				surf->shaderIndex = 0;
			} else {
				surf->shaderIndex = sh->index;
			}
			
			// now copy the vertexes.
			v = (mdrVertex_t *) (surf + 1);
			surf->ofsVerts = (int)((byte *) v - (byte *) surf);
			curv = (mdrVertex_t *) ((byte *)cursurf + LittleLong(cursurf->ofsVerts));
			
			for(j = 0; j < surf->numVerts; j++)
			{
				LL(curv->numWeights);
			
				// simple bounds check
				if(curv->numWeights < 0 || (byte *) (v + 1) + (curv->numWeights - 1) * sizeof(*weight) > (byte *) mdr + size)
				{
					ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has broken structure.\n", mod_name);
					return qfalse;
				}

				v->normal[0] = LittleFloat(curv->normal[0]);
				v->normal[1] = LittleFloat(curv->normal[1]);
				v->normal[2] = LittleFloat(curv->normal[2]);
				
				v->texCoords[0] = LittleFloat(curv->texCoords[0]);
				v->texCoords[1] = LittleFloat(curv->texCoords[1]);
				
				v->numWeights = curv->numWeights;
				weight = &v->weights[0];
				curweight = &curv->weights[0];
				
				// Now copy all the weights
				for(k = 0; k < v->numWeights; k++)
				{
					weight->boneIndex = LittleLong(curweight->boneIndex);
					weight->boneWeight = LittleFloat(curweight->boneWeight);
					
					weight->offset[0] = LittleFloat(curweight->offset[0]);
					weight->offset[1] = LittleFloat(curweight->offset[1]);
					weight->offset[2] = LittleFloat(curweight->offset[2]);
					
					weight++;
					curweight++;
				}
				
				v = (mdrVertex_t *) weight;
				curv = (mdrVertex_t *) curweight;
			}
						
			// we know the offset to the triangles now:
			tri = (mdrTriangle_t *) v;
			surf->ofsTriangles = (int)((byte *) tri - (byte *) surf);
			curtri = (mdrTriangle_t *)((byte *) cursurf + LittleLong(cursurf->ofsTriangles));
			
			// simple bounds check
			if(surf->numTriangles < 0 || (byte *) (tri + surf->numTriangles) > (byte *) mdr + size)
			{
				ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has broken structure.\n", mod_name);
				return qfalse;
			}

			for(j = 0; j < surf->numTriangles; j++)
			{
				tri->indexes[0] = LittleLong(curtri->indexes[0]);
				tri->indexes[1] = LittleLong(curtri->indexes[1]);
				tri->indexes[2] = LittleLong(curtri->indexes[2]);
				
				tri++;
				curtri++;
			}
			
			// tri now points to the end of the surface.
			surf->ofsEnd = (byte *) tri - (byte *) surf;
			surf = (mdrSurface_t *) tri;

			// find the next surface.
			cursurf = (mdrSurface_t *) ((byte *) cursurf + LittleLong(cursurf->ofsEnd));
		}

		// surf points to the next lod now.
		lod->ofsEnd = (int)((byte *) surf - (byte *) lod);
		lod = (mdrLOD_t *) surf;

		// find the next LOD.
		curlod = (mdrLOD_t *)((byte *) curlod + LittleLong(curlod->ofsEnd));
	}
	
	// lod points to the first tag now, so update the offset too.
	tag = (mdrTag_t *) lod;
	mdr->ofsTags = (int)((byte *) tag - (byte *) mdr);
	curtag = (mdrTag_t *) ((byte *)pinmodel + LittleLong(pinmodel->ofsTags));

	// simple bounds check
	if(mdr->numTags < 0 || (byte *) (tag + mdr->numTags) > (byte *) mdr + size)
	{
		ri.Printf(PRINT_WARNING, "R_LoadMDR: %s has broken structure.\n", mod_name);
		return qfalse;
	}
	
	for (i = 0 ; i < mdr->numTags ; i++)
	{
		tag->boneIndex = LittleLong(curtag->boneIndex);
		Q_strncpyz(tag->name, curtag->name, sizeof(tag->name));
		
		tag++;
		curtag++;
	}
	
	// And finally we know the real offset to the end.
	mdr->ofsEnd = (int)((byte *) tag - (byte *) mdr);

	// phew! we're done.
	
	return qtrue;
}



//=============================================================================

/*
** RE_BeginRegistration
*/
void RE_BeginRegistration( glconfig_t *glconfigOut ) {

	R_Init();

	*glconfigOut = glConfig;

	//R_IssuePendingRenderCommands();

	tr.viewCluster = -1;		// force markleafs to regenerate
	R_ClearFlares();
	RE_ClearScene();

	tr.registered = qtrue;
}

//=============================================================================

/*
===============
R_ModelInit
===============
*/
void R_ModelInit( void ) {
	model_t		*mod;

	// leave a space for NULL model
	tr.numModels = 0;

	mod = R_AllocModel();
	mod->type = MOD_BAD;
}


/*
================
R_Modellist_f
================
*/
void R_Modellist_f( void ) {
	int		i, j;
	model_t	*mod;
	int		total;
	int		lods;

	total = 0;
	for ( i = 1 ; i < tr.numModels; i++ ) {
		mod = tr.models[i];
		lods = 1;
		for ( j = 1 ; j < MD3_MAX_LODS ; j++ ) {
			if ( mod->md3[j] && mod->md3[j] != mod->md3[j-1] ) {
				lods++;
			}
		}
		ri.Printf( PRINT_ALL, "%8i : (%i) %s\n",mod->dataSize, lods, mod->name );
		total += mod->dataSize;
	}
	ri.Printf( PRINT_ALL, "%8i : Total models\n", total );

#if	0		// not working right with new hunk
	if ( tr.world ) {
		ri.Printf( PRINT_ALL, "\n%8i : %s\n", tr.world->dataSize, tr.world->name );
	}
#endif
}


//=============================================================================


/*
================
R_GetTag
================
*/
static md3Tag_t *R_GetTag( md3Header_t *mod, int frame, const char *tagName ) {
	md3Tag_t		*tag;
	int				i;

	if ( frame >= mod->numFrames ) {
		// it is possible to have a bad frame while changing models, so don't error
		frame = mod->numFrames - 1;
	}

	tag = (md3Tag_t *)((byte *)mod + mod->ofsTags) + frame * mod->numTags;
	for ( i = 0 ; i < mod->numTags ; i++, tag++ ) {
		if ( !strcmp( tag->name, tagName ) ) {
			return tag;	// found it
		}
	}

	return NULL;
}

static md3Tag_t *R_GetAnimTag( mdrHeader_t *mod, int framenum, const char *tagName, md3Tag_t * dest) 
{
	int				i, j, k;
	int				frameSize;
	mdrFrame_t		*frame;
	mdrTag_t		*tag;

	if ( framenum >= mod->numFrames ) 
	{
		// it is possible to have a bad frame while changing models, so don't error
		framenum = mod->numFrames - 1;
	}

	tag = (mdrTag_t *)((byte *)mod + mod->ofsTags);
	for ( i = 0 ; i < mod->numTags ; i++, tag++ )
	{
		if ( !strcmp( tag->name, tagName ) )
		{
			Q_strncpyz(dest->name, tag->name, sizeof(dest->name));

			// uncompressed model...
			//
			frameSize = (intptr_t)( &((mdrFrame_t *)0)->bones[ mod->numBones ] );
			frame = (mdrFrame_t *)((byte *)mod + mod->ofsFrames + framenum * frameSize );

			for (j = 0; j < 3; j++)
			{
				for (k = 0; k < 3; k++)
					dest->axis[j][k]=frame->bones[tag->boneIndex].matrix[k][j];
			}

			dest->origin[0]=frame->bones[tag->boneIndex].matrix[0][3];
			dest->origin[1]=frame->bones[tag->boneIndex].matrix[1][3];
			dest->origin[2]=frame->bones[tag->boneIndex].matrix[2][3];				

			return dest;
		}
	}

	return NULL;
}

/*
================
R_LerpTag
================
*/
int R_LerpTag( orientation_t *tag, qhandle_t handle, int startFrame, int endFrame, 
					 float frac, const char *tagName ) {
	md3Tag_t	*start, *end;
	md3Tag_t	start_space, end_space;
	int		i;
	float		frontLerp, backLerp;
	model_t		*model;

	model = R_GetModelByHandle( handle );
	if ( !model->md3[0] )
	{
		if(model->type == MOD_MDR)
		{
			start = R_GetAnimTag(model->modelData.mdr, startFrame, tagName, &start_space);
			end = R_GetAnimTag(model->modelData.mdr, endFrame, tagName, &end_space);
		}
		else if( model->type == MOD_IQM ) {
			return R_IQMLerpTag( tag, model->modelData.iqm,
					startFrame, endFrame,
					frac, tagName );
		} else {
			start = end = NULL;
		}
	}
	else
	{
		start = R_GetTag( model->md3[0], startFrame, tagName );
		end = R_GetTag( model->md3[0], endFrame, tagName );
	}

	if ( !start || !end ) {
		AxisClear( tag->axis );
		VectorClear( tag->origin );
		return qfalse;
	}

	frontLerp = frac;
	backLerp = 1.0f - frac;

	for ( i = 0 ; i < 3 ; i++ ) {
		tag->origin[i] = start->origin[i] * backLerp +  end->origin[i] * frontLerp;
		tag->axis[0][i] = start->axis[0][i] * backLerp +  end->axis[0][i] * frontLerp;
		tag->axis[1][i] = start->axis[1][i] * backLerp +  end->axis[1][i] * frontLerp;
		tag->axis[2][i] = start->axis[2][i] * backLerp +  end->axis[2][i] * frontLerp;
	}
	VectorNormalize( tag->axis[0] );
	VectorNormalize( tag->axis[1] );
	VectorNormalize( tag->axis[2] );
	return qtrue;
}


/*
====================
R_ModelBounds
====================
*/
void R_ModelBounds( qhandle_t handle, vec3_t mins, vec3_t maxs ) {
	model_t		*model;

	model = R_GetModelByHandle( handle );

	if(model->type == MOD_BRUSH) {
		VectorCopy( model->bmodel->bounds[0], mins );
		VectorCopy( model->bmodel->bounds[1], maxs );
		
		return;
	} else if (model->type == MOD_MESH) {
		md3Header_t	*header;
		md3Frame_t	*frame;

		header = model->md3[0];
		frame = (md3Frame_t *) ((byte *)header + header->ofsFrames);

		VectorCopy( frame->bounds[0], mins );
		VectorCopy( frame->bounds[1], maxs );
		
		return;
	} else if (model->type == MOD_MDR) {
		mdrHeader_t	*header;
		mdrFrame_t	*frame;

		header = model->modelData.mdr;
		frame = (mdrFrame_t *) ((byte *)header + header->ofsFrames);

		VectorCopy( frame->bounds[0], mins );
		VectorCopy( frame->bounds[1], maxs );

		return;
	} else if(model->type == MOD_IQM) {
		iqmData_t *iqmData;

		iqmData = model->modelData.iqm;

		if(iqmData->bounds)
		{
			VectorCopy(iqmData->bounds, mins);
			VectorCopy(iqmData->bounds + 3, maxs);
			return;
		}
	}

	VectorClear( mins );
	VectorClear( maxs );
}
