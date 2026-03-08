#ifndef VK_ATMOSPHERE_H
#define VK_ATMOSPHERE_H

typedef struct {
	float sunDir[4];
	float sunColor[4];
	float rayleigh[4];
	float mie[4];
	float atmParams[4];
	float viewOrigin[4];
	float viewForward[4];
	float viewRight[4];
	float viewUp[4];
	float viewParams[4];
} vkAtmospherePushConstants_t;

void vk_atmosphere_build_push_constants( vkAtmospherePushConstants_t *pc );

#endif
