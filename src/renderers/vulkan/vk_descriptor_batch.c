#include "tr_local.h"
#include "vk_descriptor_batch.h"
#include "vk.h"
#include <string.h>

// Pending descriptor write tracking
typedef struct {
    VkWriteDescriptorSet write;
    VkDescriptorImageInfo image_info_storage[16];  // Storage for image info
    VkDescriptorBufferInfo buffer_info_storage[16]; // Storage for buffer info
    VkWriteDescriptorSetAccelerationStructureKHR as_info_storage[4]; // Storage for AS info
    uint32_t image_info_count;
    uint32_t buffer_info_count;
    uint32_t as_info_count;
    qboolean valid;
} pending_descriptor_write_t;

// Batching state
static struct {
    pending_descriptor_write_t writes[MAX_PENDING_DESCRIPTOR_WRITES];
    uint32_t write_count;
    qboolean initialized;
    
    // Hash table for detecting redundant updates
    // Key: (descriptor_set << 16) | (binding << 8) | array_element
    // Value: index into writes array
    uint32_t update_hash[256];  // Simple hash table
    uint32_t hash_count;
} batch_state;

// Hash function for descriptor set + binding + array element
static uint32_t hash_descriptor_key(VkDescriptorSet dst_set, uint32_t binding, uint32_t array_element) {
    // Simple hash: combine descriptor set handle (lower 16 bits), binding, and array element
    uint32_t set_low = (uint32_t)((uintptr_t)dst_set & 0xFFFF);
    return (set_low << 16) | (binding << 8) | (array_element & 0xFF);
}

// Find existing update for same descriptor set + binding + array element
static int32_t find_existing_update(VkDescriptorSet dst_set, uint32_t binding, uint32_t array_element) {
    uint32_t key = hash_descriptor_key(dst_set, binding, array_element);
    uint32_t hash_index = key % ARRAY_LEN(batch_state.update_hash);
    
    // Linear probe for collision resolution
    for (uint32_t i = 0; i < ARRAY_LEN(batch_state.update_hash); i++) {
        uint32_t idx = (hash_index + i) % ARRAY_LEN(batch_state.update_hash);
        uint32_t write_idx = batch_state.update_hash[idx];
        
        if (write_idx == 0xFFFFFFFF) {
            break; // Empty slot
        }
        
        if (write_idx < batch_state.write_count) {
            pending_descriptor_write_t *existing = &batch_state.writes[write_idx];
            if (existing->valid &&
                existing->write.dstSet == dst_set &&
                existing->write.dstBinding == binding &&
                existing->write.dstArrayElement == array_element) {
                return (int32_t)write_idx;
            }
        }
    }
    
    return -1;
}

// Add to hash table
static void add_to_hash(uint32_t write_index) {
    pending_descriptor_write_t *write = &batch_state.writes[write_index];
    uint32_t key = hash_descriptor_key(write->write.dstSet, write->write.dstBinding, write->write.dstArrayElement);
    uint32_t hash_index = key % ARRAY_LEN(batch_state.update_hash);
    
    // Linear probe for empty slot
    for (uint32_t i = 0; i < ARRAY_LEN(batch_state.update_hash); i++) {
        uint32_t idx = (hash_index + i) % ARRAY_LEN(batch_state.update_hash);
        if (batch_state.update_hash[idx] == 0xFFFFFFFF) {
            batch_state.update_hash[idx] = write_index;
            batch_state.hash_count++;
            return;
        }
    }
}

// Initialize descriptor batching system
void vk_descriptor_batch_init(void) {
    Com_Memset(&batch_state, 0, sizeof(batch_state));
    
    // Initialize hash table
    for (uint32_t i = 0; i < ARRAY_LEN(batch_state.update_hash); i++) {
        batch_state.update_hash[i] = 0xFFFFFFFF;
    }
    
    batch_state.initialized = qtrue;
    ri.Printf(PRINT_ALL, "Vulkan: Descriptor batching system initialized\n");
}

// Shutdown descriptor batching system
void vk_descriptor_batch_shutdown(void) {
    if (!batch_state.initialized) {
        return;
    }
    
    // Flush any pending updates
    vk_descriptor_batch_flush();
    
    Com_Memset(&batch_state, 0, sizeof(batch_state));
    batch_state.initialized = qfalse;
    ri.Printf(PRINT_ALL, "Vulkan: Descriptor batching system shut down\n");
}

// Reset batching state for new frame
void vk_descriptor_batch_reset_frame(void) {
    if (!batch_state.initialized) {
        return;
    }
    
    // Flush any remaining updates from previous frame
    vk_descriptor_batch_flush();
    
    // Reset state
    batch_state.write_count = 0;
    batch_state.hash_count = 0;
    
    // Clear hash table
    for (uint32_t i = 0; i < ARRAY_LEN(batch_state.update_hash); i++) {
        batch_state.update_hash[i] = 0xFFFFFFFF;
    }
    
    // Mark all writes as invalid
    for (uint32_t i = 0; i < MAX_PENDING_DESCRIPTOR_WRITES; i++) {
        batch_state.writes[i].valid = qfalse;
    }
}

// Defer a descriptor set update (batches multiple updates together)
qboolean vk_descriptor_batch_defer_update(const VkWriteDescriptorSet *write) {
    if (!batch_state.initialized || !write || vk.device_lost) {
        return qfalse;
    }
    
    // Check for redundant update (same descriptor set + binding + array element)
    int32_t existing_idx = find_existing_update(write->dstSet, write->dstBinding, write->dstArrayElement);
    if (existing_idx >= 0) {
        // Replace existing update with new one (avoid redundant updates)
        pending_descriptor_write_t *existing = &batch_state.writes[existing_idx];
        
        // Copy the new write, preserving storage
        existing->write = *write;
        
        // Copy image info if present
        if (write->pImageInfo && write->descriptorCount > 0) {
            uint32_t copy_count = (write->descriptorCount < ARRAY_LEN(existing->image_info_storage)) 
                                  ? write->descriptorCount 
                                  : ARRAY_LEN(existing->image_info_storage);
            Com_Memcpy(existing->image_info_storage, write->pImageInfo, 
                      copy_count * sizeof(VkDescriptorImageInfo));
            existing->write.pImageInfo = existing->image_info_storage;
            existing->image_info_count = copy_count;
        } else {
            existing->image_info_count = 0;
        }
        
        // Copy buffer info if present
        if (write->pBufferInfo && write->descriptorCount > 0) {
            uint32_t copy_count = (write->descriptorCount < ARRAY_LEN(existing->buffer_info_storage)) 
                                  ? write->descriptorCount 
                                  : ARRAY_LEN(existing->buffer_info_storage);
            Com_Memcpy(existing->buffer_info_storage, write->pBufferInfo, 
                      copy_count * sizeof(VkDescriptorBufferInfo));
            existing->write.pBufferInfo = existing->buffer_info_storage;
            existing->buffer_info_count = copy_count;
        } else {
            existing->buffer_info_count = 0;
        }
        
        // Handle acceleration structure info (if present)
        if (write->pNext) {
            // For AS info, we need to handle the pNext chain
            // This is simplified - full implementation would need to handle all pNext types
            existing->write.pNext = write->pNext;
        }
        
        existing->valid = qtrue;
        return qtrue; // Successfully replaced redundant update
    }
    
    // Check if batch is full
    if (batch_state.write_count >= MAX_PENDING_DESCRIPTOR_WRITES) {
        // Flush and retry
        vk_descriptor_batch_flush();
        if (batch_state.write_count >= MAX_PENDING_DESCRIPTOR_WRITES) {
            ri.Printf(PRINT_WARNING, "vk_descriptor_batch_defer_update: Batch still full after flush\n");
            return qfalse;
        }
    }
    
    // Add new write
    uint32_t write_idx = batch_state.write_count++;
    pending_descriptor_write_t *pending = &batch_state.writes[write_idx];
    
    // Copy the write structure
    pending->write = *write;
    pending->valid = qtrue;
    pending->image_info_count = 0;
    pending->buffer_info_count = 0;
    pending->as_info_count = 0;
    
    // Copy image info if present
    if (write->pImageInfo && write->descriptorCount > 0) {
        uint32_t copy_count = (write->descriptorCount < ARRAY_LEN(pending->image_info_storage)) 
                              ? write->descriptorCount 
                              : ARRAY_LEN(pending->image_info_storage);
        Com_Memcpy(pending->image_info_storage, write->pImageInfo, 
                  copy_count * sizeof(VkDescriptorImageInfo));
        pending->write.pImageInfo = pending->image_info_storage;
        pending->image_info_count = copy_count;
    }
    
    // Copy buffer info if present
    if (write->pBufferInfo && write->descriptorCount > 0) {
        uint32_t copy_count = (write->descriptorCount < ARRAY_LEN(pending->buffer_info_storage)) 
                              ? write->descriptorCount 
                              : ARRAY_LEN(pending->buffer_info_storage);
        Com_Memcpy(pending->buffer_info_storage, write->pBufferInfo, 
                  copy_count * sizeof(VkDescriptorBufferInfo));
        pending->write.pBufferInfo = pending->buffer_info_storage;
        pending->buffer_info_count = copy_count;
    }
    
    // Handle acceleration structure info (simplified - full implementation would handle pNext chain)
    if (write->pNext) {
        pending->write.pNext = write->pNext;
    }
    
    // Add to hash table
    add_to_hash(write_idx);
    
    return qtrue;
}

// Flush all pending descriptor updates
void vk_descriptor_batch_flush(void) {
    if (!batch_state.initialized || batch_state.write_count == 0 || vk.device_lost) {
        return;
    }
    
    if (vk.device == VK_NULL_HANDLE) {
        return;
    }
    
    // Collect all valid writes
    VkWriteDescriptorSet writes[MAX_PENDING_DESCRIPTOR_WRITES];
    uint32_t valid_count = 0;
    
    for (uint32_t i = 0; i < batch_state.write_count; i++) {
        if (batch_state.writes[i].valid) {
            writes[valid_count++] = batch_state.writes[i].write;
        }
    }
    
    if (valid_count > 0) {
        // Batch update all descriptor sets in a single call
        // qvkUpdateDescriptorSets is declared in vk.c
        extern PFN_vkUpdateDescriptorSets qvkUpdateDescriptorSets;
        if (qvkUpdateDescriptorSets && vk.device != VK_NULL_HANDLE) {
            qvkUpdateDescriptorSets(vk.device, valid_count, writes, 0, NULL);
            // Note: UpdateDescriptorSets doesn't return VkResult, it's void
        }
    }
    
    // Reset state
    batch_state.write_count = 0;
    batch_state.hash_count = 0;
    
    // Clear hash table
    for (uint32_t i = 0; i < ARRAY_LEN(batch_state.update_hash); i++) {
        batch_state.update_hash[i] = 0xFFFFFFFF;
    }
    
    // Mark all writes as invalid
    for (uint32_t i = 0; i < MAX_PENDING_DESCRIPTOR_WRITES; i++) {
        batch_state.writes[i].valid = qfalse;
    }
}

// Check if there are pending updates
qboolean vk_descriptor_batch_has_pending(void) {
    return batch_state.initialized && batch_state.write_count > 0;
}

// Get count of pending updates
uint32_t vk_descriptor_batch_get_pending_count(void) {
    if (!batch_state.initialized) {
        return 0;
    }
    return batch_state.write_count;
}
