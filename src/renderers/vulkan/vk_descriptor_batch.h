#ifndef VK_DESCRIPTOR_BATCH_H
#define VK_DESCRIPTOR_BATCH_H

#include "tr_local.h"
#include "vk.h"

#ifdef __cplusplus
extern "C" {
#endif

// Maximum number of pending descriptor writes per frame
#define MAX_PENDING_DESCRIPTOR_WRITES 256

// Initialize descriptor batching system
void vk_descriptor_batch_init(void);

// Shutdown descriptor batching system
void vk_descriptor_batch_shutdown(void);

// Reset batching state for new frame
void vk_descriptor_batch_reset_frame(void);

// Defer a descriptor set update (batches multiple updates together)
// Returns qtrue if successfully queued, qfalse if batch is full
qboolean vk_descriptor_batch_defer_update(const VkWriteDescriptorSet *write);

// Flush all pending descriptor updates
// This should be called before binding descriptor sets or at frame boundaries
void vk_descriptor_batch_flush(void);

// Check if there are pending updates
qboolean vk_descriptor_batch_has_pending(void);

// Get count of pending updates
uint32_t vk_descriptor_batch_get_pending_count(void);

#ifdef __cplusplus
}
#endif

#endif // VK_DESCRIPTOR_BATCH_H
