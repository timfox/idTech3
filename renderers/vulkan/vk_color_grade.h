#pragma once


/*
 * Raster Ultra 1.10 — color grade ownership (CDL / LUT / lift-gamma-gain).
 * Grades in documented working space; does not replace lighting.
 */

typedef struct vkColorGradeState_s {
	float slope[3];
	float offset[3];
	float power[3];
	float saturation;
	float contrast;
	float temperature;
	float tint;
	qboolean lutActive;
	qboolean volumeBlend;
} vkColorGradeState_t;

void vk_color_grade_register_cvars( void );
void vk_color_grade_init( void );
void vk_color_grade_shutdown( void );

qboolean vk_color_grade_active( void );
const vkColorGradeState_t *vk_color_grade_state( void );

void vk_color_grade_status_f( void );

