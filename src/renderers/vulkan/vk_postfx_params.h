#ifndef VK_POSTFX_PARAMS_H
#define VK_POSTFX_PARAMS_H

#include <stdint.h>

typedef struct {
	float invViewProj[16];
	float prevViewProj[16];
	float viewMatrix[16];
	float motionBlur[4];   /* enabled, strength, samples, maxRadius */
	float depthOfField[4]; /* enabled, aperture, focusDistance, focusRange */
	float frameInfo[4];    /* dofMaxBlur, texelSize.x, texelSize.y, motionValid */
	float depthParams[2];  /* zNear, zFar for linear depth and velocity */
} VkPostFXParams;

void vk_update_postfx_params( uint32_t cmd_index );

#endif
