/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

MD3-tag / sidecar ragdoll bind helpers for Soft Step.
===========================================================================
*/

#pragma once

#include "phys_bullet.h"
#include "phys_procedural_anim.h"
#include "phys_motor.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Load models/<path>.rag (or path ending in .rag). Returns qtrue on success.
   Empty numBones → caller uses procedural 11-bone layout. */
qboolean Phys_RagdollLoadDef( const char *pathOrModel, physRagdollDef_t *out );

/* Sample MD3 tags for a frame and drive Soft Step anim targets (matching bone tagName). */
qboolean Phys_RagdollApplyMd3Frame( physRagdollHandle_t handle, const physRagdollDef_t *bind,
	const char *md3Path, int frame );

typedef struct physBoundRagdoll_s {
	physRagdollHandle_t ragdoll;
	procAnimHandle_t    anim;
	physMotorHandle_t   motor;
} physBoundRagdoll_t;

/* Death / spawn helper: load .rag (optional), create Soft Step ragdoll + ProcAnim + motor.
   startDead=qtrue calls ProcAnim_Kill (classic death path). Map Euphoria uses qfalse. */
qboolean Phys_RagdollSpawnBoundEx( const char *modelOrRag, const vec3_t origin, physBoundRagdoll_t *out,
	qboolean startDead );
qboolean Phys_RagdollSpawnBound( const char *modelOrRag, const vec3_t origin, physBoundRagdoll_t *out );

#ifdef __cplusplus
}
#endif
