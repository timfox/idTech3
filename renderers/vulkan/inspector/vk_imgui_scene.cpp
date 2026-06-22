/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "../tr_local.h"
#include "vk_imgui_scene.h"

int VkImgScene_RefEntityCount( void )
{
	return tr.refdef.num_entities;
}

qboolean VkImgScene_RefEntitySnapshot( int index, vkImgSceneRefEntity_t *out )
{
	const trRefEntity_t *ent;
	shader_t *sh;
	int c, cap;

	if ( !out || index < 0 || !tr.refdef.entities || index >= tr.refdef.num_entities ) {
		return qfalse;
	}
	ent = &tr.refdef.entities[index];
	Com_Memset( out, 0, sizeof( *out ) );
	{
		model_t *mdl;

		mdl = R_GetModelByHandle( ent->e.hModel );
		if ( mdl ) {
			out->modelModType = (int)mdl->type;
		} else {
			out->modelModType = MOD_BAD;
		}
	}
	out->reType = (int)ent->e.reType;
	out->renderfx = ent->e.renderfx;
	out->hModel = ent->e.hModel;
	out->origin[0] = ent->e.origin[0];
	out->origin[1] = ent->e.origin[1];
	out->origin[2] = ent->e.origin[2];
	out->frame = ent->e.frame;
	out->oldframe = ent->e.oldframe;
	out->backlerp = ent->e.backlerp;
	out->customShader = ent->e.customShader;
	out->customSkin = ent->e.customSkin;
	out->ambientLight[0] = ent->ambientLight[0];
	out->ambientLight[1] = ent->ambientLight[1];
	out->ambientLight[2] = ent->ambientLight[2];
	out->directedLight[0] = ent->directedLight[0];
	out->directedLight[1] = ent->directedLight[1];
	out->directedLight[2] = ent->directedLight[2];
	out->lightDir[0] = ent->lightDir[0];
	out->lightDir[1] = ent->lightDir[1];
	out->lightDir[2] = ent->lightDir[2];

	sh = R_GetShaderByHandle( ent->e.customShader );
	if ( sh && sh->name[0] ) {
		Q_strncpyz( out->customShaderName, sh->name, sizeof( out->customShaderName ) );
	}

	out->morphChannelCount = ent->morphChannelCount;
	cap = ent->morphChannelCount;
	if ( cap > IQM_MORPH_MAX_CHANNELS ) {
		cap = IQM_MORPH_MAX_CHANNELS;
	}
	if ( cap > VK_IMGUI_SCENE_MORPH_MAX ) {
		cap = VK_IMGUI_SCENE_MORPH_MAX;
	}
	for ( c = 0; c < cap; c++ ) {
		out->morphHashes[c] = ent->morphChannelHashes[c];
		out->morphWeights[c] = ent->morphChannelWeights[c];
		out->morphWeightPrev[c] = ent->morphChannelWeightPrev[c];
	}
	return qtrue;
}

void VkImgScene_RefEntityModelPath( int index, char *buf, int bufSize )
{
	const trRefEntity_t *ent;
	model_t *mod;

	if ( !buf || bufSize <= 0 ) {
		return;
	}
	buf[0] = '\0';
	if ( index < 0 || !tr.refdef.entities || index >= tr.refdef.num_entities ) {
		return;
	}
	ent = &tr.refdef.entities[index];
	mod = R_GetModelByHandle( ent->e.hModel );
	if ( mod && mod->name[0] ) {
		Q_strncpyz( buf, mod->name, bufSize );
	}
}

unsigned VkImgScene_DlightCount( void )
{
	return tr.refdef.num_dlights;
}

qboolean VkImgScene_DlightSnapshot( int index, vkImgSceneDlight_t *out )
{
	const dlight_t *dl;

	if ( !out || index < 0 || !tr.refdef.dlights || (unsigned)index >= tr.refdef.num_dlights ) {
		return qfalse;
	}
	dl = &tr.refdef.dlights[index];
	out->origin[0] = dl->origin[0];
	out->origin[1] = dl->origin[1];
	out->origin[2] = dl->origin[2];
	out->origin2[0] = dl->origin2[0];
	out->origin2[1] = dl->origin2[1];
	out->origin2[2] = dl->origin2[2];
	out->radius = dl->radius;
	out->color[0] = dl->color[0];
	out->color[1] = dl->color[1];
	out->color[2] = dl->color[2];
	out->additive = dl->additive ? qtrue : qfalse;
	out->linear = dl->linear ? qtrue : qfalse;
	return qtrue;
}

qboolean VkImgScene_WorldLoaded( void )
{
	return ( tr.world != NULL ) ? qtrue : qfalse;
}

void VkImgScene_WorldName( char *buf, int bufSize )
{
	if ( !buf || bufSize <= 0 ) {
		return;
	}
	buf[0] = '\0';
	if ( tr.world && tr.world->name[0] ) {
		Q_strncpyz( buf, tr.world->name, bufSize );
	}
}

void VkImgScene_WorldBaseName( char *buf, int bufSize )
{
	if ( !buf || bufSize <= 0 ) {
		return;
	}
	buf[0] = '\0';
	if ( tr.world && tr.world->baseName[0] ) {
		Q_strncpyz( buf, tr.world->baseName, bufSize );
	}
}

int VkImgScene_WorldNumSurfaces( void )
{
	return tr.world ? tr.world->numsurfaces : 0;
}

int VkImgScene_WorldNumClusters( void )
{
	return tr.world ? tr.world->numClusters : 0;
}

int VkImgScene_WorldClusterBytes( void )
{
	return tr.world ? tr.world->clusterBytes : 0;
}

int VkImgScene_WorldNumMapShaders( void )
{
	return tr.world ? tr.world->numShaders : 0;
}

int VkImgScene_WorldNumBModels( void )
{
	return tr.world ? tr.world->numBModels : 0;
}

void VkImgScene_RefdefViewport( int *x, int *y, int *w, int *h )
{
	if ( x ) {
		*x = tr.refdef.x;
	}
	if ( y ) {
		*y = tr.refdef.y;
	}
	if ( w ) {
		*w = tr.refdef.width;
	}
	if ( h ) {
		*h = tr.refdef.height;
	}
}

void VkImgScene_RefdefFov( float *fov_x, float *fov_y )
{
	if ( fov_x ) {
		*fov_x = tr.refdef.fov_x;
	}
	if ( fov_y ) {
		*fov_y = tr.refdef.fov_y;
	}
}

int VkImgScene_RefdefTime( void )
{
	return tr.refdef.time;
}

int VkImgScene_RefdefRdFlags( void )
{
	return tr.refdef.rdflags;
}

void VkImgScene_RefdefViewOrg( float out[3] )
{
	if ( !out ) {
		return;
	}
	out[0] = tr.refdef.vieworg[0];
	out[1] = tr.refdef.vieworg[1];
	out[2] = tr.refdef.vieworg[2];
}

void VkImgScene_RefdefViewAxis0( float out[3] )
{
	if ( !out ) {
		return;
	}
	out[0] = tr.refdef.viewaxis[0][0];
	out[1] = tr.refdef.viewaxis[0][1];
	out[2] = tr.refdef.viewaxis[0][2];
}

int VkImgScene_RefdefNumDrawSurfs( void )
{
	return tr.refdef.numDrawSurfs;
}

int VkImgScene_RefdefNumLitSurfs( void )
{
	return tr.refdef.numLitSurfs;
}

int VkImgScene_NumSortedShaders( void )
{
	return tr.numShaders;
}

qboolean VkImgScene_SortedShaderSnapshot( int sortedIndex, vkImgSceneShader_t *out )
{
	shader_t *sh;

	if ( !out || sortedIndex < 0 || sortedIndex >= tr.numShaders ) {
		return qfalse;
	}
	sh = tr.sortedShaders[sortedIndex];
	if ( !sh ) {
		return qfalse;
	}
	Com_Memset( out, 0, sizeof( *out ) );
	Q_strncpyz( out->name, sh->name, sizeof( out->name ) );
	out->indexInternal = sh->index;
	out->sortedIndex = sortedIndex;
	out->numUnfoggedPasses = sh->numUnfoggedPasses;
	out->surfaceFlags = sh->surfaceFlags;
	out->contentFlags = sh->contentFlags;
	out->cullType = (int)sh->cullType;
	out->isSky = sh->isSky ? qtrue : qfalse;
	out->explicitlyDefined = sh->explicitlyDefined ? qtrue : qfalse;
	out->entityMergable = sh->entityMergable ? qtrue : qfalse;
	return qtrue;
}

int VkImgScene_NumModels( void )
{
	return tr.numModels;
}

qboolean VkImgScene_ModelSlot( int modelIndex, vkImgSceneModelSlot_t *out )
{
	model_t *mod;

	if ( !out || modelIndex < 1 || modelIndex >= tr.numModels ) {
		return qfalse;
	}
	mod = tr.models[modelIndex];
	if ( !mod ) {
		return qfalse;
	}
	Com_Memset( out, 0, sizeof( *out ) );
	Q_strncpyz( out->name, mod->name, sizeof( out->name ) );
	out->modType = (int)mod->type;
	return qtrue;
}
