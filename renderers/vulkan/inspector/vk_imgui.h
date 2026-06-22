/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

ImGui Inspector for Vulkan renderer.
Mirrors the architecture of EternalJK's pbr-rtx-inspector by Sunny JK.
Provides dockable editor with object browser, property inspector,
shader editor, GPU profiler, and PBR debug render modes.
===========================================================================
*/

#ifndef VK_IMGUI_H
#define VK_IMGUI_H

#ifdef USE_IMGUI

#ifdef __cplusplus
extern "C" {
#endif

/* Avoid including vk.h/tr_local.h in this header due to C/C++ keyword conflicts.
   Use uint64_t for Vulkan handle types. */
#include <stdint.h>

#define INSPECTOR_OT_ENTITY     (1)
#define INSPECTOR_OT_SHADER     (2)
#define INSPECTOR_OT_SURFACE    (4)
#define INSPECTOR_OT_NODE       (8)
#define INSPECTOR_OT_FLARE      (16)
#define INSPECTOR_OT_LIGHT      (32)

typedef struct {
	qboolean    initialized;
	char        searchKeyword[MAX_QPATH];
	qboolean    outlineSelected;
	int         numShaders;

	struct {
		int             index;
		uint64_t        image;
	} renderMode;

	struct {
		uint32_t    type;
		void       *ptr;
		void       *prev;
	} selected;

	struct {
		qboolean    active;
		vec3_t     *origin;
		float      *radius;
		float      *rotation;
	} transform;

	struct {
		qboolean    active;
		int         index;
	} shader;

	struct {
		qboolean    active;
		void       *ptr;
	} entity;

	struct {
		qboolean    active;
		void       *node;
	} node;

	struct {
		qboolean    active;
		void       *surf;
	} surface;

	/* Scene / inspector selection (indexes into current tr.refdef; kind disambiguates) */
	int			inspectorSelectionKind; /* 0=none, 1=world BSP, 2=ref entity, 3=dynamic light */
	int			inspectorEntityIndex;
	int			inspectorDlightIndex;
} vkImguiInspector_t;

#define VK_INSP_KIND_NONE	0
#define VK_INSP_KIND_WORLD	1
#define VK_INSP_KIND_ENTITY	2
#define VK_INSP_KIND_DLIGHT	3

typedef struct {
	struct {
		qboolean    open;
		float       width;
		float       height;
	} viewport;

	struct {
		qboolean    open;
		qboolean    textMode;
		int         index;
		int         prev;
	} shader;

	struct {
		qboolean    open;
	} profiler;

	struct {
		qboolean    open;
	} postfx;

	struct {
		qboolean    open;
	} physics;

	struct {
		qboolean    open;
	} volumetrics;

	struct {
		qboolean    open;
	} objects;

	struct {
		qboolean    open;
	} inspector;

	/* id Studio-inspired in-engine tools (gated by r_studio_tools) */
	struct {
		qboolean    open;
	} studioMap;

	struct {
		qboolean    open;
	} studioConsole;

	struct {
		qboolean    open;
	} studioEntities;
} vkImguiWindows_t;

typedef struct {
	qboolean    inputState;
	qboolean    active;
} vkImguiGlobal_t;

extern vkImguiInspector_t   vkInspector;
extern vkImguiWindows_t     vkWindows;
extern vkImguiGlobal_t      vkImguiState;

void VkImgui_Initialize(void);
void VkImgui_Shutdown(void);
void VkImgui_BeginFrame(void);
void VkImgui_Draw(void);
void VkImgui_SwapchainRestarted(void);

void VkImgui_DrawObjects(void);
void VkImgui_DrawInspector(void);
void VkImgui_DrawProfiler(void);
void VkImgui_DrawSimRenderDebugHud(void);
void VkImgui_DrawViewport(void);
void VkImgui_DrawShaderEditor(void);
void VkImgui_DrawPostFXPanel(void);
void VkImgui_DrawPhysicsPanel(void);
void VkImgui_DrawVolumetricsPanel(void);
void VkImgui_DrawStudioMapPanel(void);
void VkImgui_DrawStudioConsolePanel(void);
void VkImgui_DrawStudioEntitiesPanel(void);
void VkImgui_DrawStudioAnimationPanel(void);

void VkImgui_BindGameColorImage(void);

/* Record ImGui into the swapchain (overlay_compose pass). Called from vk_frame_end after gamma. */
void VkImgui_RecordOverlayPass( void );

qboolean VkImgui_IsVulkanBackendReady( void );
void VkImgui_SetVulkanBackendReady( qboolean ready );

void VkImgui_NotifySwapchainRestart( void );

#ifdef __cplusplus
void VkImgui_ApplyInspectorStyle( void );
void VkImgui_LoadFonts( void );
#endif

#ifdef __cplusplus
}
/* C++-only inspector chrome (ImGui DockBuilder, style). */
void VkImgui_DrawInspectorChrome( void );
void VkImgui_ResetInspectorWorkspaceLayout( void );
#endif

#endif /* USE_IMGUI */
#endif /* VK_IMGUI_H */
