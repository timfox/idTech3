// Real TLAS wiring placeholder (in-progress)
#include "tr_local.h"
#include "vk_rtx_acceleration.h"
extern "C" {
// Real TLAS builder placeholder. Returns success for now.
qboolean vk_rtx_build_tlas_real(VkCommandBuffer cmd_buffer) {
    ri.Printf(PRINT_DEVELOPER, "TLAS real build (stub) called\n");
    (void)cmd_buffer;
    return qtrue;
}
}
