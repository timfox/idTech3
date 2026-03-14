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
	float depthParams[4];  /* zNear, zFar for linear depth and velocity */
	float toneMapParams0[4];   /* toe, shoulder, whitePoint, blackClip */
	float toneMapParams1[4];   /* highlightDesat, contrast, contrastPivot, legacyTonemapMode */
	float colorBalance[4];     /* temperature, tint, exposureBias, preExposureScale */
	float colorGrade[4];       /* saturation, vibrance, legacyContrast, legacySaturation */
	float shadowsLift[4];      /* rgb lift, unused */
	float midsGamma[4];        /* rgb gamma, unused */
	float highlightsGain[4];   /* rgb gain, unused */
	float splitShadow[4];      /* rgb tint, balance */
	float splitHighlight[4];   /* rgb tint, strength */
	float lensEffects0[4];     /* vignette, vignetteRadius, chromaticAberration, filmGrain */
	float lensEffects1[4];     /* outlineStrength, outlineThreshold, filmLook, sharpen */
	float runtimeFlags[4];     /* greyscale, dither, postDebug, postEnabled */
	float lutParams[4];        /* lutIntensity, lutEnabled, lutStripDim, reserved */
	float autoExposureParams[4]; /* avgLogLum, targetLum, minExposure, maxExposure */
	float localExposureParams[4]; /* enabled, strength, shadowClampEV, highlightClampEV */
} VkPostFXParams;

typedef struct {
	float lowPercent;
	float highPercent;
	float centerWeight;
	float reserved;
} VkLuminancePushConstants;

void vk_update_postfx_params( uint32_t cmd_index );

#endif
