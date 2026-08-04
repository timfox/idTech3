#pragma once

#include "tr_local.h"

/* Editable 2D stroke payload. Positions are in the owning overlay/canvas
 * coordinate system; arcLength is written by the resampling stage. */
typedef struct {
	float x, y;
	float radius;
	float opacity;
	float arcLength;
} vkVectorBrushPoint_t;

typedef struct {
	uint32_t firstPoint;
	uint32_t pointCount;
	uint32_t brushIndex;
	uint32_t layer;
} vkVectorBrushStroke_t;

typedef struct {
	uint32_t strokeCount;
	uint32_t pointCount;
	uint32_t stampCapacity;
	uint32_t owner;       /* 0 inactive, 1 vector overlay sidecar */
	uint32_t target;      /* 1 = overlay target; never SceneHDR/G-buffer */
	uint32_t generation;
	uint32_t fillMarkers;
	uint32_t airbrushContinuous;
} vkVectorBrushContract_t;

void vk_vector_brush_init( void );
void vk_vector_brush_shutdown( void );
void vk_vector_brush_status_f( void );
const vkVectorBrushContract_t *vk_vector_brush_contract( void );

