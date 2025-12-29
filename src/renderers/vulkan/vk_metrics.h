#pragma once

// Lightweight runtime metrics for Vulkan components.
// These hooks are intentionally minimal to avoid impacting hot paths.

// If VK_METRICS_ENABLED is not defined by the build, provide no-op inline stubs
// to avoid linker dependencies and allow compilation without linking the metrics
// implementation.
#ifndef VK_METRICS_ENABLED
static inline void vk_metrics_increment_pipeline_alloc(void) { (void)0; }
static inline void vk_metrics_increment_pipeline_lookup(void) { (void)0; }
static inline void vk_metrics_increment_renderpass_alloc(uint32_t renderPassIndex, uint32_t pipeline_index) {
  (void)renderPassIndex; (void)pipeline_index;
}
static inline void vk_metrics_increment_surface_created(void) { }
static inline void vk_metrics_report(void) { }
#endif

void vk_metrics_increment_pipeline_alloc(void);
void vk_metrics_increment_renderpass_alloc(uint32_t renderPassIndex, uint32_t pipeline_index);
void vk_metrics_increment_pipeline_lookup(void);
void vk_metrics_increment_surface_created(void);
void vk_metrics_report(void);

