#pragma once

/*
 * Color Pipeline Phase 2.4 — HDR resolve / SceneHDR integrity for WBOIT.
 * Freeze resolve chain ownership: fog_scene opaque base, empty-pixel policy,
 * scene-linear (not pre-exposed), generation matching.
 * See docs/HDR_RESOLVE_INTEGRITY.md.
 */

#ifdef USE_VULKAN

#include "../common/tr_types.h"

#define HDR_RESOLVE_CONTRACT_VERSION 1u

typedef struct hdrResolveContract_s {
	qboolean resolveInSceneLinearHdr;   /* SCENE_LINEAR_HDR only */
	qboolean resolveBeforeExposure;     /* never into pre-exposed / display */
	qboolean emptyPreservesOpaque;      /* empty accum → C_opaque, never black */
	qboolean opaqueFromFogSceneCopy;    /* resolve samples fog_scene, not live color mid-write */
	qboolean noSecondFogOnResolve;      /* fog ownership: T only in accum when mode≥1 */
	qboolean requireExtentMatch;        /* opaque/accum/reveal same extent */
	qboolean requireOitGenMatch;        /* attachment gen == descriptor gen */
	qboolean requireFogSceneCopyThisFrame;

	uint32_t contractVersion;
	uint32_t contractHash;
} hdrResolveContract_t;

const hdrResolveContract_t *vk_hdr_resolve_contract_get( void );
uint32_t vk_hdr_resolve_contract_compute_hash( const hdrResolveContract_t *c );
qboolean vk_hdr_resolve_contract_validate( const hdrResolveContract_t *c, char *errBuf, int errBufSize );
void vk_hdr_resolve_contract_print( const hdrResolveContract_t *c );
void vk_hdr_resolve_contract_register( void );

/* Per-frame / resource generation bookkeeping. */
void vk_hdr_resolve_begin_frame( void );
void vk_hdr_resolve_note_fog_scene_copy( uint32_t width, uint32_t height );
void vk_hdr_resolve_note_scene_hdr_recreate( void );
void vk_hdr_resolve_note_depth_recreate( void );
/* requireFogCopy: qtrue only when validating immediately before OIT resolve. */
qboolean vk_hdr_resolve_runtime_validate( qboolean requireFogCopy, char *errBuf, int errBufSize );
void vk_hdr_resolve_status_f( void );

uint32_t vk_hdr_resolve_fog_scene_generation( void );
uint32_t vk_hdr_resolve_scene_hdr_generation( void );
uint32_t vk_hdr_resolve_depth_generation( void );

#endif /* USE_VULKAN */
