#pragma once

#ifdef USE_VULKAN

/*
 * Raster Ultra 1.10 — cinematic / physical camera model.
 * Gameplay FOV stays separate. DOF/MB must exclude UI; weapon policy explicit.
 */

typedef struct vkCinematicCamera_s {
	float focalLengthMm;
	float sensorWidthMm;
	float apertureF;
	float focusDistance;
	float shutterAngleDeg;
	float iso;
	float anamorphicRatio;
	float cropFactor;
	qboolean dofEnabled;
	qboolean motionBlurEnabled;
	qboolean excludeWeapon;
	qboolean excludeUI;
	qboolean lowLatencyDisableMB;
	qboolean gameplayMode; /* softer cinematic */
} vkCinematicCamera_t;

void vk_cinematic_camera_register_cvars( void );
void vk_cinematic_camera_init( void );
void vk_cinematic_camera_shutdown( void );

qboolean vk_cinematic_camera_active( void );
const vkCinematicCamera_t *vk_cinematic_camera_state( void );

/* Apply opt-in DOF/MB cvars only when cinematic camera module owns them. */
void vk_cinematic_camera_begin_frame( void );

void vk_cinematic_camera_status_f( void );

#endif /* USE_VULKAN */
