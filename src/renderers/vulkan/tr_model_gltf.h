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

typedef struct gltfPrimitive_s {
	gltfVertex_t   *vertices;
	int             numVertices;
	uint32_t       *indices;
	int             numIndices;
	int             materialIndex;
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
	int     jointIndex;
	int     type;
	float  *times;
	float  *values;
	int     numKeyframes;
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
void     R_FreeGLTF(gltfModel_t *model);

qboolean R_RegisterGLTF(const char *name, model_t *mod);

#ifdef __cplusplus
}
#endif
