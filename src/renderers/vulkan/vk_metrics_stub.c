#include <stdint.h>
#include "vk_metrics.h"

// Weak-implemented fallbacks for metrics hooks.
// If the real implementations are compiled into vk_metrics.c, they will override these.
__attribute__((weak)) void vk_metrics_increment_pipeline_alloc(void) { }
__attribute__((weak)) void vk_metrics_increment_pipeline_lookup(void) { }
__attribute__((weak)) void vk_metrics_increment_renderpass_alloc(uint32_t renderPassIndex, uint32_t pipeline_index) { (void)renderPassIndex; (void)pipeline_index; }
__attribute__((weak)) void vk_metrics_increment_surface_created(void) { }
__attribute__((weak)) void vk_metrics_report(void) { }
