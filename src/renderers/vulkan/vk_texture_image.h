/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

2D/cubemap texture VkImage creation, staging uploads, and sampler
descriptor writes (split from vk.c).
===========================================================================
*/

#ifndef VK_TEXTURE_IMAGE_H
#define VK_TEXTURE_IMAGE_H

#include "../common/vulkan/vulkan.h"
#include "tr_common.h"

void vk_create_image( image_t *image, int width, int height, int mip_levels );
void vk_upload_image_data( image_t *image, int x, int y, int width, int height, int miplevels, byte *pixels, int size, qboolean update );
void vk_upload_cubemap_mip_data( image_t *image, int face_size, int miplevels, const byte *pixels, int size, int bytes_per_pixel, qboolean update );
void vk_upload_compressed_image_data( image_t *image, int width, int height, int miplevels, byte *pixels, int size, qboolean update );
void vk_update_descriptor_set( image_t *image, qboolean mipmap );
void vk_destroy_image_resources( VkImage *image, VkImageView *imageView );

#endif
