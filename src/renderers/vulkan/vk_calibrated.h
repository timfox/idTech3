#ifndef VK_CALIBRATED_H
#define VK_CALIBRATED_H

#include <stdint.h>
#include <stdbool.h>

// Optional per-frame/per-pass timestamping using VK_EXT_calibrated_timestamps

bool vk_calibrated_supported(void);
void vk_calibrated_begin_frame(uint32_t frameIndex, const char* label);
void vk_calibrated_end_frame(uint32_t frameIndex, const char* label, uint64_t duration_ns);
void vk_calibrated_begin_pass(const char* name);
void vk_calibrated_end_pass(const char* name, uint64_t duration_ns);

#endif // VK_CALIBRATED_H

