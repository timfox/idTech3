/*
===========================================================================
id Tech 3 - RTX Renderer Header

RTX renderer header with public API declarations
===========================================================================
*/

#ifndef __VK_RTX_H__
#define __VK_RTX_H__

#include "../../renderercommon/tr_public.h"
#include "../../renderercommon/tr_types.h"

// RTX-specific CVARs
extern cvar_t *r_rtx_enable;
extern cvar_t *r_rtx_mode;          // 0=hardware RT, 1=compute RT, 2=hybrid
extern cvar_t *r_rtx_samples;
extern cvar_t *r_rtx_bounces;
extern cvar_t *r_rtx_denoise;
extern cvar_t *r_rtx_god_rays;
extern cvar_t *r_rtx_atmosphere;
extern cvar_t *r_rtx_ibl;
extern cvar_t *r_rtx_fsr;
extern cvar_t *r_rtx_raymarching;
extern cvar_t *r_rtx_imgui;        // Show ImGui settings window

// RTX renderer statistics (internal use only)

// Public RTX API
qboolean RTX_Init(void);
void RTX_Shutdown(refShutdownCode_t code);
void RTX_BeginFrame(stereoFrame_t stereoFrame);
void RTX_RenderScene(const refdef_t *fd);
void RTX_EndFrame(int *frontEndMsec, int *backEndMsec);

// ImGui support (implemented in vk_rtx_main.c)
qboolean RTX_ImGuiBackendInit(void);
void RTX_ImGuiBackendShutdown(void);
void RTX_ImGuiBackendNewFrame(void);
void RTX_ImGuiBackendRenderDrawData(const struct ImDrawData *drawData);

// Entry point
refexport_t* RTX_GetRefAPI(int apiVersion, refimport_t* rimp);

#endif // __VK_RTX_H__