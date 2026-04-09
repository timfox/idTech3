/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

glTF 2.0 model loader using cgltf (MIT license).
Supports PBR materials, skeletal animation, morph targets,
and multi-LOD meshes from standard glTF/GLB files.
===========================================================================
*/

#pragma once

#include "../common/tr_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GLTF_MAX_JOINTS      128
#define GLTF_MAX_MORPH_TARGETS 8
#define GLTF_MAX_MESHES       256
#define GLTF_MAX_MATERIALS    64

typedef struct gltfVertex_s {
	vec3_t  position;
	vec3_t  normal;
	vec4_t  tangent;
	vec2_t  texCoord0;
	vec2_t  texCoord1;
	vec4_t  color;
	byte    joints[4];
	vec4_t  weights;
} gltfVertex_t;

/* Morph target: delta position/normal per vertex */
typedef struct gltfMorphTarget_s {
	char    name[MAX_QPATH]; /* from mesh target_names when present */
	float *deltaPosition;  /* numVertices * 3, or NULL */
	float *deltaNormal;   /* numVertices * 3, or NULL */
	float *deltaTangent;  /* numVertices * 3, or NULL (optional) */
} gltfMorphTarget_t;

typedef struct gltfPrimitive_s {
	gltfVertex_t   *vertices;
	int             numVertices;
	uint32_t       *indices;
	int             numIndices;
	int             materialIndex;
	int             numMorphTargets;
	gltfMorphTarget_t *morphTargets;  /* [numMorphTargets], NULL if none */
} gltfPrimitive_t;

typedef struct gltfMesh_s {
	char            name[MAX_QPATH];
	gltfPrimitive_t *primitives;
	int              numPrimitives;
} gltfMesh_t;

typedef struct gltfMaterial_s {
	char    name[MAX_QPATH];

	vec4_t  baseColorFactor;
	float   metallicFactor;
	float   roughnessFactor;
	vec3_t  emissiveFactor;
	float   emissiveStrength;

	char    baseColorTexture[MAX_QPATH];
	char    normalTexture[MAX_QPATH];
	char    metallicRoughnessTexture[MAX_QPATH];
	char    emissiveTexture[MAX_QPATH];
	char    occlusionTexture[MAX_QPATH];

	float   alphaCutoff;
	int     alphaMode;
	qboolean doubleSided;

	float   clearcoatFactor;
	float   sheenRoughnessFactor;
	vec3_t  sheenColorFactor;
	float   transmissionFactor;
	float   ior;
} gltfMaterial_t;

typedef struct gltfJoint_s {
	char    name[MAX_QPATH];
	int     parent;
	vec3_t  translation;
	vec4_t  rotation;
	vec3_t  scale;
	float   inverseBindMatrix[16];
} gltfJoint_t;

typedef struct gltfSkeleton_s {
	gltfJoint_t joints[GLTF_MAX_JOINTS];
	int         numJoints;
} gltfSkeleton_t;

typedef struct gltfAnimChannel_s {
	int     jointIndex;   /* skin joint index, or -1 */
	int     morphMeshIndex; /* model mesh index for morph weight animation, or -1 */
	int     type;         /* cgltf_animation_path_type_* */
	float  *times;
	float  *values;
	int     numKeyframes;
	int     componentsPerKeyframe; /* 3 translation, 4 rotation, 3 scale, N weights */
} gltfAnimChannel_t;

typedef struct gltfAnimation_s {
	char                name[MAX_QPATH];
	gltfAnimChannel_t  *channels;
	int                 numChannels;
	float               duration;
} gltfAnimation_t;

typedef struct gltfModel_s {
	gltfMesh_t       meshes[GLTF_MAX_MESHES];
	int              numMeshes;

	gltfMaterial_t   materials[GLTF_MAX_MATERIALS];
	int              numMaterials;

	gltfSkeleton_t   skeleton;

	gltfAnimation_t *animations;
	int              numAnimations;

	vec3_t           boundsMin;
	vec3_t           boundsMax;
} gltfModel_t;

qboolean R_LoadGLTF(const char *filename, gltfModel_t *model);
/* animIndex < 0 or out of range: bind pose. timeSeconds loops by clip duration. */
void     R_ComputeGLTFJointMatrices(const gltfModel_t *model, int animIndex, float timeSeconds, float *outMatrices);
/* blendTowardB: 0 = pose A only, 1 = pose B only (linear TRS + slerp rotation). Use for refEntity backlerp. */
void     R_ComputeGLTFJointMatricesBlend(const gltfModel_t *model, int animA, float timeA, int animB, float timeB,
	float blendTowardB, float *outMatrices);
/* Fills outWeights[0..numTargets-1] for one mesh; returns qtrue if any weight channel affected this mesh. */
qboolean R_SampleGLTFMeshMorphWeights(const gltfModel_t *model, int animIndex, float timeSeconds,
	int meshIndex, float *outWeights, int numTargets);

/* Returns gltfModel_t* from modelData when model is MOD_GLTF, else NULL */
const gltfModel_t *R_GetGLTFModelFromModelData(const void *modelData);
void     R_FreeGLTF(gltfModel_t *model);

/* R_RegisterGLTF, R_AddGLTFSurfaces, R_GLTFModelBounds declared in tr_local.h */

#ifdef __cplusplus
}
#endif
