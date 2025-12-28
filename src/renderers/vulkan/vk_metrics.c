#include <stdio.h>
#include "vk_metrics.h"

// Simple atomic-ish counters using plain increment (single-threaded during init)
static unsigned long long g_pipeline_alloc_count = 0;
static unsigned long long g_surface_created_count = 0;

void vk_metrics_increment_pipeline_alloc(void) {
    g_pipeline_alloc_count++;
    // Lightweight periodic log to avoid flood
    if ((g_pipeline_alloc_count % 1000) == 0) {
        fprintf(stderr, "[VK_METRICS] pipeline_alloc_count=%llu\n", g_pipeline_alloc_count);
    }
}

void vk_metrics_increment_surface_created(void) {
    g_surface_created_count++;
    if ((g_surface_created_count % 100) == 0) {
        fprintf(stderr, "[VK_METRICS] surface_created_count=%llu\n", g_surface_created_count);
    }
}

void vk_metrics_report(void) {
    fprintf(stderr, "[VK_METRICS] REPORT - pipelines=%llu, surfaces=%llu\n",
            g_pipeline_alloc_count, g_surface_created_count);
}

