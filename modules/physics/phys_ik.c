/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

IK solver implementation.
Two-bone IK for limbs, CCD for chains, specialized foot/aim/look-at.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "phys_ik.h"
#include <math.h>

void IK_QuatFromAxisAngle(const vec3_t axis, float angle, vec4_t out) {
	float ha = angle * 0.5f;
	float s = sinf(ha);
	out[0] = axis[0] * s;
	out[1] = axis[1] * s;
	out[2] = axis[2] * s;
	out[3] = cosf(ha);
}

void IK_QuatMultiply(const vec4_t a, const vec4_t b, vec4_t out) {
	out[0] = a[3]*b[0] + a[0]*b[3] + a[1]*b[2] - a[2]*b[1];
	out[1] = a[3]*b[1] - a[0]*b[2] + a[1]*b[3] + a[2]*b[0];
	out[2] = a[3]*b[2] + a[0]*b[1] - a[1]*b[0] + a[2]*b[3];
	out[3] = a[3]*b[3] - a[0]*b[0] - a[1]*b[1] - a[2]*b[2];
}

void IK_QuatSlerp(const vec4_t a, const vec4_t b, float t, vec4_t out) {
	float dot = a[0]*b[0] + a[1]*b[1] + a[2]*b[2] + a[3]*b[3];
	vec4_t nb;
	float theta, sinTheta, wa, wb;
	int i;

	if (dot < 0) {
		for (i = 0; i < 4; i++) nb[i] = -b[i];
		dot = -dot;
	} else {
		for (i = 0; i < 4; i++) nb[i] = b[i];
	}

	if (dot > 0.9995f) {
		for (i = 0; i < 4; i++) out[i] = a[i] + t * (nb[i] - a[i]);
		float len = sqrtf(out[0]*out[0] + out[1]*out[1] + out[2]*out[2] + out[3]*out[3]);
		if (len > 0) for (i = 0; i < 4; i++) out[i] /= len;
		return;
	}

	theta = acosf(dot);
	sinTheta = sinf(theta);
	wa = sinf((1.0f - t) * theta) / sinTheta;
	wb = sinf(t * theta) / sinTheta;
	for (i = 0; i < 4; i++) out[i] = wa * a[i] + wb * nb[i];
}

void IK_QuatRotatePoint(const vec4_t q, const vec3_t p, vec3_t out) {
	float ix =  q[3]*p[0] + q[1]*p[2] - q[2]*p[1];
	float iy =  q[3]*p[1] + q[2]*p[0] - q[0]*p[2];
	float iz =  q[3]*p[2] + q[0]*p[1] - q[1]*p[0];
	float iw = -q[0]*p[0] - q[1]*p[1] - q[2]*p[2];

	out[0] = ix*q[3] + iw*(-q[0]) + iy*(-q[2]) - iz*(-q[1]);
	out[1] = iy*q[3] + iw*(-q[1]) + iz*(-q[0]) - ix*(-q[2]);
	out[2] = iz*q[3] + iw*(-q[2]) + ix*(-q[1]) - iy*(-q[0]);
}

qboolean IK_SolveTwoBone(const vec3_t root, const vec3_t target, const vec3_t poleVector,
                          float upperLen, float lowerLen,
                          vec3_t outMid, vec3_t outEnd) {
	vec3_t toTarget, toTargetNorm, toPole, perpPole, midDir;
	float distSq, dist, cosAngle, angle, midDist;

	VectorSubtract(target, root, toTarget);
	distSq = DotProduct(toTarget, toTarget);
	dist = sqrtf(distSq);

	float maxReach = upperLen + lowerLen;
	float minReach = fabsf(upperLen - lowerLen);

	if (dist >= maxReach) {
		VectorCopy(toTarget, toTargetNorm);
		VectorNormalize(toTargetNorm);
		VectorMA(root, upperLen, toTargetNorm, outMid);
		VectorMA(root, maxReach, toTargetNorm, outEnd);
		return qfalse;
	}

	if (dist <= minReach) {
		VectorCopy(toTarget, toTargetNorm);
		VectorNormalize(toTargetNorm);
		VectorMA(root, upperLen, toTargetNorm, outMid);
		VectorCopy(target, outEnd);
		return qfalse;
	}

	cosAngle = (upperLen * upperLen + distSq - lowerLen * lowerLen) / (2.0f * upperLen * dist);
	if (cosAngle > 1.0f) cosAngle = 1.0f;
	if (cosAngle < -1.0f) cosAngle = -1.0f;
	angle = acosf(cosAngle);

	VectorCopy(toTarget, toTargetNorm);
	VectorNormalize(toTargetNorm);

	VectorSubtract(poleVector, root, toPole);
	float projLen = DotProduct(toPole, toTargetNorm);
	VectorMA(toPole, -projLen, toTargetNorm, perpPole);
	VectorNormalize(perpPole);

	midDist = cosAngle * upperLen;
	float perpDist = sinf(angle) * upperLen;
	VectorScale(toTargetNorm, midDist, midDir);
	VectorMA(midDir, perpDist, perpPole, midDir);
	VectorAdd(root, midDir, outMid);

	VectorCopy(target, outEnd);
	return qtrue;
}

qboolean IK_SolveCCD(ikChain_t *chain) {
	int iter, i;
	vec3_t toEnd, toTarget, axis;
	float dot, angle, dist;

	if (!chain || chain->numBones < 2) return qfalse;

	for (iter = 0; iter < chain->maxIterations; iter++) {
		int lastBone = chain->numBones - 1;
		dist = Distance(chain->bones[lastBone].position, chain->target);
		if (dist < chain->tolerance) return qtrue;

		for (i = lastBone - 1; i >= 0; i--) {
			VectorSubtract(chain->bones[lastBone].position, chain->bones[i].position, toEnd);
			VectorSubtract(chain->target, chain->bones[i].position, toTarget);

			float lenEnd = VectorLength(toEnd);
			float lenTarget = VectorLength(toTarget);
			if (lenEnd < 0.001f || lenTarget < 0.001f) continue;

			VectorScale(toEnd, 1.0f / lenEnd, toEnd);
			VectorScale(toTarget, 1.0f / lenTarget, toTarget);

			dot = DotProduct(toEnd, toTarget);
			if (dot > 0.9999f) continue;
			if (dot < -0.9999f) dot = -0.9999f;

			angle = acosf(dot) * chain->weight;
			CrossProduct(toEnd, toTarget, axis);
			VectorNormalize(axis);

			vec4_t rot;
			IK_QuatFromAxisAngle(axis, angle, rot);
			IK_QuatMultiply(rot, chain->bones[i].rotation, chain->bones[i].rotation);

			int j;
			for (j = i + 1; j <= lastBone; j++) {
				vec3_t offset;
				VectorSubtract(chain->bones[j].position, chain->bones[i].position, offset);
				IK_QuatRotatePoint(rot, offset, offset);
				VectorAdd(chain->bones[i].position, offset, chain->bones[j].position);
			}
		}
	}
	return qfalse;
}

qboolean IK_SolveFootPlacement(ikFootPlacement_t *foot) {
	vec3_t kneeHint;
	if (!foot) return qfalse;

	VectorCopy(foot->kneeHint, kneeHint);
	if (VectorLength(kneeHint) < 0.01f) {
		VectorSet(kneeHint, 0, 1, 0);
	}

	vec3_t footTarget;
	VectorCopy(foot->footTarget, footTarget);
	footTarget[2] += foot->groundOffset;

	return IK_SolveTwoBone(foot->hipPosition, footTarget, kneeHint,
		foot->upperLegLen, foot->lowerLegLen,
		foot->resultKnee, foot->resultFoot);
}

qboolean IK_SolveAim(ikAimIK_t *aim, float dt) {
	vec3_t toTarget, currentAim, axis;
	float angle, dot, maxRot;

	if (!aim) return qfalse;

	VectorSubtract(aim->aimTarget, aim->bonePosition, toTarget);
	VectorNormalize(toTarget);

	VectorCopy(aim->aimAxis, currentAim);
	IK_QuatRotatePoint(aim->boneRotation, currentAim, currentAim);

	dot = DotProduct(currentAim, toTarget);
	if (dot > 0.9999f) {
		Vector4Copy(aim->boneRotation, aim->resultRotation);
		return qtrue;
	}
	if (dot < -1.0f) dot = -1.0f;

	angle = acosf(dot);
	maxRot = aim->maxAngle * (3.14159f / 180.0f);
	if (angle > maxRot) angle = maxRot;

	float step = aim->speed * dt;
	if (step > angle) step = angle;

	CrossProduct(currentAim, toTarget, axis);
	if (VectorLength(axis) < 0.0001f) {
		Vector4Copy(aim->boneRotation, aim->resultRotation);
		return qfalse;
	}
	VectorNormalize(axis);

	vec4_t deltaRot;
	IK_QuatFromAxisAngle(axis, step, deltaRot);
	IK_QuatMultiply(deltaRot, aim->boneRotation, aim->resultRotation);
	return qtrue;
}

qboolean IK_SolveLookAt(ikLookAt_t *look, float dt) {
	ikAimIK_t aim;
	vec3_t fwd = {0, 0, 1};
	float maxAngle;

	if (!look) return qfalse;

	maxAngle = look->maxYaw > look->maxPitch ? look->maxYaw : look->maxPitch;

	VectorCopy(look->headPosition, aim.bonePosition);
	Vector4Copy(look->headRotation, aim.boneRotation);
	VectorCopy(look->lookTarget, aim.aimTarget);
	VectorCopy(fwd, aim.aimAxis);
	aim.maxAngle = maxAngle;
	aim.speed = look->speed;

	qboolean result = IK_SolveAim(&aim, dt);
	Vector4Copy(aim.resultRotation, look->resultRotation);
	return result;
}
