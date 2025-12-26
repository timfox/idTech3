#ifndef __VK_IMAGES_H__
#define __VK_IMAGES_H__

#include "vk.h"

#ifdef __cplusplus
extern "C" {
#endif

// Image and texture management functions
void vk_create_image(image_t *image, int width, int height, int mip_levels);
void vk_create_image_view(image_t *image, VkImageViewType view_type, VkImageAspectFlags aspect);
void vk_destroy_image(image_t *image);
void vk_upload_image_data(image_t *image, int x, int y, int width, int height, int layers, const void *data, int data_size, qboolean update);

#ifdef __cplusplus
}
#endif

#endif // __VK_IMAGES_H__
