#include <stdio.h>
#include "vk_metrics.h"
#include "vk.h"

// Simple atomic-ish counters using plain increment (single-threaded during init)
static unsigned long long g_pipeline_alloc_count = 0;
static unsigned long long g_renderpass_alloc_counts[RENDER_PASS_COUNT] = {0};
static unsigned long long g_pipeline_lookup_count = 0;
static unsigned long long g_surface_created_count = 0;

void vk_metrics_increment_pipeline_alloc(void) {
    g_pipeline_alloc_count++;
    // Lightweight periodic log to avoid flood
    if ((g_pipeline_alloc_count % 1000) == 0) {
        fprintf(stderr, "[VK_METRICS] pipeline_alloc_count=%llu\n", g_pipeline_alloc_count);
    }
}

void vk_metrics_increment_pipeline_lookup(void) {
    g_pipeline_lookup_count++;
    if ((g_pipeline_lookup_count % 1000) == 0) {
        fprintf(stderr, "[VK_METRICS] pipeline_lookup_count=%llu\n", g_pipeline_lookup_count);
    }
}

void vk_metrics_increment_surface_created(void) {
    g_surface_created_count++;
    if ((g_surface_created_count % 100) == 0) {
        fprintf(stderr, "[VK_METRICS] surface_created_count=%llu\n", g_surface_created_count);
    }
}

void vk_metrics_report(void) {
    fprintf(stderr, "[VK_METRICS] REPORT - pipelines_alloc=%llu, pipelines_lookup=%llu, surfaces=%llu\n",
            g_pipeline_alloc_count, g_pipeline_lookup_count, g_surface_created_count);
    for (int i = 0; i < RENDER_PASS_COUNT; ++i) {
        fprintf(stderr, "[VK_METRICS] renderpass_alloc[%d]=%llu\n", i, g_renderpass_alloc_counts[i]);
    }
}

void vk_metrics_increment_renderpass_alloc(uint32_t renderPassIndex, uint32_t pipeline_index) {
    (void)pipeline_index; // currently unused in per-pass accounting, reserved for future correlation
    if (renderPassIndex < (uint32_t)RENDER_PASS_COUNT) {
        g_renderpass_alloc_counts[renderPassIndex]++;
        // Periodic micro-log for visibility
        if ((g_renderpass_alloc_counts[renderPassIndex] % 1000) == 0) {
            fprintf(stderr, "[VK_METRICS] renderpass_alloc[%u] (pipeline_index=%u) count=%llu\n",
                    renderPassIndex, pipeline_index, g_renderpass_alloc_counts[renderPassIndex]);
        }
    }
}
