/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Post-process push constant layout shared by pipeline layout creation (vk_init_device.c)
and gamma/TAA recording (vk_frame_end.c).
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	float paniniAmount;
	float paniniD;
	float paniniS;
	float aspect;
	float fovXDeg;
	float paniniBorderMode;
	float paniniDebugMode;
	float brightness;
	float paniniZoom;
	float paniniPad0;
	float paniniPad1;
	float paniniMask; /* 1=apply panini, 0=skip (console/UI) */
	float exposure;
	float srcUVAlignPad[3];
	float srcUVScaleBias[4];
} VkPostProcessPushConstants;

#ifdef __cplusplus
}
#endif
