/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Read-only snapshots of tr.refdef / tr.world for Dear ImGui panels.
Compiled as C++ with -fno-operator-names (tr_local.h uses `or` as a field name).
===========================================================================
*/

#ifndef VK_IMGUI_SCENE_H
#define VK_IMGUI_SCENE_H

#include "../../../qcommon/q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VK_IMGUI_SCENE_MORPH_MAX 8

typedef struct {
	int		reType;
	int		renderfx;
	int		hModel;
	float		origin[3];
	int		frame;
	int		oldframe;
	float		backlerp;
	int		customShader;
	int		customSkin;
	int		modelModType;
	char		customShaderName[MAX_QPATH];
	float		ambientLight[3];
	float		directedLight[3];
	float		lightDir[3];
	int		morphChannelCount;
	unsigned	morphHashes[VK_IMGUI_SCENE_MORPH_MAX];
	float		morphWeights[VK_IMGUI_SCENE_MORPH_MAX];
	float		morphWeightPrev[VK_IMGUI_SCENE_MORPH_MAX];
} vkImgSceneRefEntity_t;

typedef struct {
	float		origin[3];
	float		origin2[3];
	float		radius;
	float		color[3];
	qboolean	additive;
	qboolean	linear;
} vkImgSceneDlight_t;

typedef struct {
	char		name[MAX_QPATH];
	int		indexInternal;
	int		sortedIndex;
	int		numUnfoggedPasses;
	int		surfaceFlags;
	int		contentFlags;
	int		cullType;
	qboolean	isSky;
	qboolean	explicitlyDefined;
	qboolean	entityMergable;
} vkImgSceneShader_t;

typedef struct {
	char		name[MAX_QPATH];
	int		modType;
} vkImgSceneModelSlot_t;

int VkImgScene_RefEntityCount( void );
qboolean VkImgScene_RefEntitySnapshot( int index, vkImgSceneRefEntity_t *out );
void VkImgScene_RefEntityModelPath( int index, char *buf, int bufSize );

unsigned VkImgScene_DlightCount( void );
qboolean VkImgScene_DlightSnapshot( int index, vkImgSceneDlight_t *out );

qboolean VkImgScene_WorldLoaded( void );
void VkImgScene_WorldName( char *buf, int bufSize );
void VkImgScene_WorldBaseName( char *buf, int bufSize );
int VkImgScene_WorldNumSurfaces( void );
int VkImgScene_WorldNumClusters( void );
int VkImgScene_WorldClusterBytes( void );
int VkImgScene_WorldNumMapShaders( void );
int VkImgScene_WorldNumBModels( void );

void VkImgScene_RefdefViewport( int *x, int *y, int *w, int *h );
void VkImgScene_RefdefFov( float *fov_x, float *fov_y );
int VkImgScene_RefdefTime( void );
int VkImgScene_RefdefRdFlags( void );
void VkImgScene_RefdefViewOrg( float out[3] );
void VkImgScene_RefdefViewAxis0( float out[3] );
int VkImgScene_RefdefNumDrawSurfs( void );
int VkImgScene_RefdefNumLitSurfs( void );

int VkImgScene_NumSortedShaders( void );
qboolean VkImgScene_SortedShaderSnapshot( int sortedIndex, vkImgSceneShader_t *out );

int VkImgScene_NumModels( void );
/* index 1..numModels-1 typical; writes model path + MOD_* type */
qboolean VkImgScene_ModelSlot( int modelIndex, vkImgSceneModelSlot_t *out );

#ifdef __cplusplus
}
#endif

#endif /* VK_IMGUI_SCENE_H */
