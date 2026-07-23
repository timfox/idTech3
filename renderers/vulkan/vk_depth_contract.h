#pragma once

/*
 * Color Pipeline Phase 2.3.1 — authoritative depth contract freeze.
 * Shared by opaque, transparent, fog, OIT weighting, soft particles, refraction.
 * See docs/DEPTH_CONTRACT.md.
 *
 * Do not change fields without bumping DEPTH_CONTRACT_VERSION and updating docs.
 */

#ifdef USE_VULKAN

#include "../common/tr_types.h"

#define DEPTH_CONTRACT_VERSION 1u

typedef enum depthProjectionMode_e {
	DEPTH_PROJECTION_PERSPECTIVE = 0,
	DEPTH_PROJECTION_ORTHOGRAPHIC,
	DEPTH_PROJECTION_PERSPECTIVE_INFINITE
} depthProjectionMode_t;

/*
 * Positive view-depth: meters along the view ray from the camera.
 * Production fog / OIT must not use raw device depth as this quantity.
 */
typedef enum positiveViewDepthMode_e {
	VIEW_DEPTH_NEG_VIEW_Z = 0,       /* certified: -viewSpace.z (camera forward) */
	VIEW_DEPTH_CAMERA_DISTANCE,      /* |worldPos - viewOrg| — legacy diagnostic only */
	VIEW_DEPTH_DEVICE_RAW            /* forbidden for fog/weight — diagnostic only */
} positiveViewDepthMode_t;

typedef enum depthFarPlaneMode_e {
	DEPTH_FAR_FINITE = 0,
	DEPTH_FAR_INFINITE
} depthFarPlaneMode_t;

typedef struct depthContract_s {
	qboolean reversedZ;
	qboolean zeroToOneClipDepth;
	qboolean infiniteFarPlane;

	depthProjectionMode_t projectionMode;
	depthFarPlaneMode_t farPlaneMode;
	positiveViewDepthMode_t positiveViewDepthMode;

	float nearPlane;                 /* r_znear default 8 */
	float farPlane;                  /* viewParms.zFar at fill time; 0 = dynamic */
	float clearDepth;                /* reversed-Z clear = 0 */

	uint32_t depthCompareOp;         /* VK_COMPARE_OP_GREATER_OR_EQUAL */
	uint32_t deviceDepthFormatHint;  /* typical D32_SFLOAT / D24 — informational */

	/* Ownership bookkeeping (names filled at print / begin_frame). */
	qboolean currentDepthOwnedByScene;
	qboolean previousDepthOwnedByTemporal;

	uint32_t contractVersion;
	uint32_t contractHash;
} depthContract_t;

/* Frozen production Vulkan depth contract. */
const depthContract_t *vk_depth_contract_get( void );

/* Refresh near/far from live cvars / view (does not change hash of frozen defaults). */
void vk_depth_contract_refresh_planes( depthContract_t *dst );

uint32_t vk_depth_contract_compute_hash( const depthContract_t *c );
qboolean vk_depth_contract_validate( const depthContract_t *c, char *errBuf, int errBufSize );

void vk_depth_contract_print( const depthContract_t *c );
void vk_depth_contract_register( void );
void vk_depth_contract_begin_frame( void );
void vk_depth_contract_note_writer( const char *passName );
void vk_depth_contract_note_reader( const char *passName );

const char *vk_depth_projection_mode_name( depthProjectionMode_t m );
const char *vk_depth_view_depth_mode_name( positiveViewDepthMode_t m );
const char *vk_depth_far_plane_mode_name( depthFarPlaneMode_t m );

/* CPU mirrors of depth_view.glsl (Phase 2.3.2). */
float vk_depth_linearize_reversed_z( float deviceDepth, float zNear, float zFar );
float vk_depth_positive_view_from_world( const vec3_t worldPos, const vec3_t viewOrg,
	const vec3_t viewForward );
float vk_depth_camera_distance( const vec3_t worldPos, const vec3_t viewOrg );
float vk_depth_view_depth_to_traditional01( float viewDepth, float zNear, float zFar );

#endif /* USE_VULKAN */
