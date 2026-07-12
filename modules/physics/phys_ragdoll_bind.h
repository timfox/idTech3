/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

MD3-tag / sidecar ragdoll bind helpers for Soft Step.
===========================================================================
*/

#pragma once

#include "phys_bullet.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Load models/<path>.rag (or path ending in .rag). Returns qtrue on success.
   Empty numBones → caller uses procedural 11-bone layout. */
qboolean Phys_RagdollLoadDef( const char *pathOrModel, physRagdollDef_t *out );

#ifdef __cplusplus
}
#endif
