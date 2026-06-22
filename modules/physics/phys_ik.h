/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Inverse Kinematics solver for procedural animation.
Provides two-bone IK (limbs), CCD IK (chains), and
specialized solvers for foot placement, aim, and look-at.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../qcommon/q_shared.h"

typedef struct ikBone_s {
	vec3_t  position;
	vec4_t  rotation;
	float   length;
	int     parent;
} ikBone_t;

typedef struct ikChain_s {
	ikBone_t *bones;
	int       numBones;
	vec3_t    target;
	vec3_t    poleVector;
	float     weight;
	int       maxIterations;
	float     tolerance;
} ikChain_t;

typedef struct ikFootPlacement_s {
	vec3_t  hipPosition;
	vec3_t  footTarget;
	vec3_t  kneeHint;
	float   upperLegLen;
	float   lowerLegLen;
	float   groundOffset;
	vec3_t  resultKnee;
	vec3_t  resultFoot;
	vec4_t  resultHipRot;
	vec4_t  resultKneeRot;
} ikFootPlacement_t;

typedef struct ikAimIK_s {
	vec3_t  bonePosition;
	vec4_t  boneRotation;
	vec3_t  aimTarget;
	vec3_t  aimAxis;
	float   maxAngle;
	float   speed;
	vec4_t  resultRotation;
} ikAimIK_t;

typedef struct ikLookAt_s {
	vec3_t  headPosition;
	vec4_t  headRotation;
	vec3_t  lookTarget;
	float   maxYaw;
	float   maxPitch;
	float   speed;
	vec4_t  resultRotation;
} ikLookAt_t;

qboolean IK_SolveTwoBone(const vec3_t root, const vec3_t target, const vec3_t poleVector,
                          float upperLen, float lowerLen,
                          vec3_t outMid, vec3_t outEnd);

qboolean IK_SolveCCD(ikChain_t *chain);

qboolean IK_SolveFootPlacement(ikFootPlacement_t *foot);
qboolean IK_SolveAim(ikAimIK_t *aim, float dt);
qboolean IK_SolveLookAt(ikLookAt_t *look, float dt);

void IK_QuatFromAxisAngle(const vec3_t axis, float angle, vec4_t out);
void IK_QuatMultiply(const vec4_t a, const vec4_t b, vec4_t out);
void IK_QuatSlerp(const vec4_t a, const vec4_t b, float t, vec4_t out);
void IK_QuatRotatePoint(const vec4_t q, const vec3_t p, vec3_t out);

#ifdef __cplusplus
}
#endif
