/*
=============================================================================
id Tech 3 - Multi-Stage Path Tracer Header

Ports a multi-stage path tracing architecture to id Tech 3.
=============================================================================
*/

#pragma once

#ifdef USE_VULKAN_RAY_TRACING

// Initialize multi-stage path tracer
void PT_Q2RTX_Init(void);

// Shutdown multi-stage path tracer
void PT_Q2RTX_Shutdown(void);

// Render using multi-stage path tracer
void PT_Q2RTX_Render(uint32_t width, uint32_t height);

// Update path tracer state from CVARs
void PT_Q2RTX_UpdateCVARs(void);

// Get path tracer statistics
void PT_Q2RTX_GetStats(uint64_t *total_rays, uint64_t *total_bounces);

#endif // USE_VULKAN_RAY_TRACING
