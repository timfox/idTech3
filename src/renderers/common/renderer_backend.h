/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Shared renderer backend identifiers for dlopen plugins and roadmap targets.
See docs/RENDERERS_FUTURE.md, docs/WEBGPU_ROADMAP.md.
===========================================================================
*/
#ifndef RENDERER_BACKEND_H
#define RENDERER_BACKEND_H

#include "q_shared.h"

typedef enum {
	RENDERER_BACKEND_VULKAN = 0,
	RENDERER_BACKEND_METAL,
	RENDERER_BACKEND_DXR,
	RENDERER_BACKEND_WEBGPU_WASM,
	RENDERER_BACKEND_UNKNOWN
} rendererBackendId_t;

static ID_INLINE rendererBackendId_t R_BackendIdFromName( const char *name )
{
	if ( !name || !name[0] ) {
		return RENDERER_BACKEND_UNKNOWN;
	}
	if ( !Q_stricmp( name, "vulkan" ) ) {
		return RENDERER_BACKEND_VULKAN;
	}
	if ( !Q_stricmp( name, "metal" ) ) {
		return RENDERER_BACKEND_METAL;
	}
	if ( !Q_stricmp( name, "dxr" ) || !Q_stricmp( name, "dx12" ) ) {
		return RENDERER_BACKEND_DXR;
	}
	if ( !Q_stricmp( name, "webgpu" ) || !Q_stricmp( name, "wasm" ) ) {
		return RENDERER_BACKEND_WEBGPU_WASM;
	}
	return RENDERER_BACKEND_UNKNOWN;
}

static ID_INLINE qboolean R_BackendIsDlopenPlugin( rendererBackendId_t id )
{
	return id == RENDERER_BACKEND_VULKAN
		|| id == RENDERER_BACKEND_METAL
		|| id == RENDERER_BACKEND_DXR;
}

static ID_INLINE qboolean R_BackendIsShipping( rendererBackendId_t id )
{
	return id == RENDERER_BACKEND_VULKAN;
}

static ID_INLINE const char *R_BackendDisplayName( rendererBackendId_t id )
{
	switch ( id ) {
	case RENDERER_BACKEND_VULKAN: return "Vulkan";
	case RENDERER_BACKEND_METAL: return "Metal";
	case RENDERER_BACKEND_DXR: return "DXR";
	case RENDERER_BACKEND_WEBGPU_WASM: return "WebGPU (Wasm)";
	default: return "unknown";
	}
}

static ID_INLINE const char *R_BackendPluginBasename( rendererBackendId_t id )
{
	switch ( id ) {
	case RENDERER_BACKEND_VULKAN: return "vulkan";
	case RENDERER_BACKEND_METAL: return "metal";
	case RENDERER_BACKEND_DXR: return "dxr";
	default: return NULL;
	}
}

#endif /* RENDERER_BACKEND_H */
