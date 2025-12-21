/*
===========================================================================
Copyright (C) 2024 id Tech 3

TIKI model loader implementation.
Inspired by Ritual Entertainment's ÜberTools TIKI model system.
===========================================================================
*/

#include "tr_local.h"
#include "tr_tiki.h"

#define LL(x) x=LittleLong(x)

/*
=================
R_LoadTIKI
=================
Load a TIKI model from memory buffer
=================
*/
qboolean R_LoadTIKI(model_t *mod, void *buffer, int filesize, const char *mod_name)
{
	tikiHeader_t	*pinmodel, *tiki;
	tikiBone_t		*bone;
	tikiFrame_t		*frame;
	tikiSurface_t	*surf;
	tikiData_t		*tikiData;
	int				i, j, size;
	int				version;
	
	if (!mod || !buffer || (size_t)filesize < sizeof(tikiHeader_t))
		return qfalse;
	
	pinmodel = (tikiHeader_t *)buffer;
	
	// Check ident
	if (LittleLong(pinmodel->ident) != TIKI_IDENT) {
		ri.Printf(PRINT_WARNING, "R_LoadTIKI: %s has wrong ident\n", mod_name);
		return qfalse;
	}
	
	version = LittleLong(pinmodel->version);
	if (version != TIKI_VERSION) {
		ri.Printf(PRINT_WARNING, "R_LoadTIKI: %s has wrong version (%i should be %i)\n",
			mod_name, version, TIKI_VERSION);
		return qfalse;
	}
	
	size = LittleLong(pinmodel->ofsEnd);
	if (size > filesize) {
		ri.Printf(PRINT_WARNING, "R_LoadTIKI: %s has corrupted header\n", mod_name);
		return qfalse;
	}
	
	mod->type = MOD_TIKI;
	mod->dataSize += size;
	tikiData = (tikiData_t *)ri.Hunk_Alloc(sizeof(tikiData_t), h_low);
	mod->modelData.tiki = tikiData;
	
	// Allocate and copy header
	tiki = (tikiHeader_t *)ri.Hunk_Alloc(size, h_low);
	Com_Memcpy(tiki, buffer, size);
	
	// Fix endianness
	LL(tiki->ident);
	LL(tiki->version);
	LL(tiki->numBones);
	LL(tiki->numFrames);
	LL(tiki->numSurfaces);
	LL(tiki->numTags);
	LL(tiki->ofsBones);
	LL(tiki->ofsFrames);
	LL(tiki->ofsSurfaces);
	LL(tiki->ofsTags);
	LL(tiki->ofsEnd);
	
	tikiData->header = tiki;
	tikiData->numBones = tiki->numBones;
	tikiData->numFrames = tiki->numFrames;
	tikiData->numSurfaces = tiki->numSurfaces;
	tikiData->numTags = tiki->numTags;
	
	// Load bones
	if (tiki->numBones > 0 && tiki->ofsBones > 0) {
		tikiData->bones = (tikiBone_t *)((byte *)tiki + tiki->ofsBones);
		for (i = 0; i < tiki->numBones; i++) {
		bone = &tikiData->bones[i];
		LL(bone->parent);
		bone->position[0] = LittleFloat(bone->position[0]);
		bone->position[1] = LittleFloat(bone->position[1]);
		bone->position[2] = LittleFloat(bone->position[2]);
		bone->rotation[0] = LittleFloat(bone->rotation[0]);
		bone->rotation[1] = LittleFloat(bone->rotation[1]);
		bone->rotation[2] = LittleFloat(bone->rotation[2]);
		bone->rotation[3] = LittleFloat(bone->rotation[3]);
		bone->scale[0] = LittleFloat(bone->scale[0]);
		bone->scale[1] = LittleFloat(bone->scale[1]);
		bone->scale[2] = LittleFloat(bone->scale[2]);
		}
	}
	
	// Load frames
	if (tiki->numFrames > 0 && tiki->ofsFrames > 0) {
		tikiData->frames = (tikiFrame_t *)((byte *)tiki + tiki->ofsFrames);
		for (i = 0; i < tiki->numFrames; i++) {
			frame = &tikiData->frames[i];
			if (tiki->numBones > 0) {
				frame->bones = (tikiBone_t *)((byte *)frame + sizeof(tikiFrame_t));
			}
		}
	}
	
	// Load surfaces
	if (tiki->numSurfaces > 0 && tiki->ofsSurfaces > 0) {
		tikiData->surfaces = (tikiSurface_t *)((byte *)tiki + tiki->ofsSurfaces);
		surf = tikiData->surfaces;
		for (i = 0; i < tiki->numSurfaces; i++) {
			LL(surf->numVerts);
			LL(surf->numTris);
			LL(surf->numBoneWeights);
			LL(surf->ofsVerts);
			LL(surf->ofsTris);
			LL(surf->ofsBoneWeights);
			LL(surf->ofsEnd);
			
			// Fix vertex data endianness
			if (surf->ofsVerts > 0) {
				tikiVertex_t *vert = (tikiVertex_t *)((byte *)surf + surf->ofsVerts);
				for (j = 0; j < surf->numVerts; j++) {
					vert[j].position[0] = LittleFloat(vert[j].position[0]);
					vert[j].position[1] = LittleFloat(vert[j].position[1]);
					vert[j].position[2] = LittleFloat(vert[j].position[2]);
					vert[j].normal[0] = LittleFloat(vert[j].normal[0]);
					vert[j].normal[1] = LittleFloat(vert[j].normal[1]);
					vert[j].normal[2] = LittleFloat(vert[j].normal[2]);
					vert[j].st[0] = LittleFloat(vert[j].st[0]);
					vert[j].st[1] = LittleFloat(vert[j].st[1]);
				}
			}
			
			// Fix triangle data endianness
			if (surf->ofsTris > 0) {
				tikiTriangle_t *tri = (tikiTriangle_t *)((byte *)surf + surf->ofsTris);
				for (j = 0; j < surf->numTris; j++) {
					LL(tri[j].indices[0]);
					LL(tri[j].indices[1]);
					LL(tri[j].indices[2]);
				}
			}
			
			// Fix bone weight data endianness
			if (surf->ofsBoneWeights > 0) {
				tikiBoneWeight_t *weight = (tikiBoneWeight_t *)((byte *)surf + surf->ofsBoneWeights);
				for (j = 0; j < surf->numBoneWeights; j++) {
					LL(weight[j].boneIndex);
					weight[j].weight = LittleFloat(weight[j].weight);
				}
			}
			
			surf = (tikiSurface_t *)((byte *)surf + surf->ofsEnd);
		}
	}
	
	// Load tags
	if (tiki->numTags > 0 && tiki->ofsTags > 0) {
		tikiData->tags = (tikiTag_t *)((byte *)tiki + tiki->ofsTags);
		for (i = 0; i < tiki->numTags; i++) {
			tikiTag_t *tag = &tikiData->tags[i];
			tag->origin[0] = LittleFloat(tag->origin[0]);
			tag->origin[1] = LittleFloat(tag->origin[1]);
			tag->origin[2] = LittleFloat(tag->origin[2]);
			tag->angles[0] = LittleFloat(tag->angles[0]);
			tag->angles[1] = LittleFloat(tag->angles[1]);
			tag->angles[2] = LittleFloat(tag->angles[2]);
			tag->angles[3] = LittleFloat(tag->angles[3]);
		}
	}
	
	ri.Printf(PRINT_DEVELOPER, "R_LoadTIKI: Loaded %s (%d bones, %d frames, %d surfaces)\n",
		mod_name, tiki->numBones, tiki->numFrames, tiki->numSurfaces);
	
	return qtrue;
}

/*
=================
R_RegisterTIKI
=================
Register a TIKI model file
=================
*/
qhandle_t R_RegisterTIKI(const char *name, model_t *mod)
{
	union {
		uint32_t *u;
		void *v;
	} buf;
	uint32_t ident;
	qboolean loaded = qfalse;
	int filesize;
	
	filesize = ri.FS_ReadFile(name, &buf.v);
	if (!buf.v) {
		mod->type = MOD_BAD;
		return 0;
	}
	
	if ((size_t)filesize < sizeof(ident)) {
		ri.FS_FreeFile(buf.v);
		mod->type = MOD_BAD;
		return 0;
	}
	
	ident = LittleLong(*buf.u);
	if (ident == TIKI_IDENT) {
		loaded = R_LoadTIKI(mod, buf.v, filesize, name);
	}
	
	ri.FS_FreeFile(buf.v);
	
	if (!loaded) {
		ri.Printf(PRINT_WARNING, "R_RegisterTIKI: couldn't load %s\n", name);
		mod->type = MOD_BAD;
		return 0;
	}
	
	return mod->index;
}

