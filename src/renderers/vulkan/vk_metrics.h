#pragma once

// Lightweight runtime metrics for Vulkan components.
// These hooks are intentionally minimal to avoid impacting hot paths.

void vk_metrics_increment_pipeline_alloc(void);
void vk_metrics_increment_surface_created(void);
void vk_metrics_report(void);

