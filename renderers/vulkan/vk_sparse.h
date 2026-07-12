#ifndef VK_SPARSE_H
#define VK_SPARSE_H

/*
===========================================================================
Chocolate sparse VkImage residency helpers (page pool + vkQueueBindSparse).
Used by virtual textures; not routed through the dense image chunk allocator.
===========================================================================
*/

#include "tr_local.h"

#define VK_SPARSE_MAX_PAGES 256

typedef struct vkSparseImage_s {
	VkImage              image;
	VkImageView          view;
	VkDescriptorSet      descriptor;
	VkFormat             format;
	int                  width;
	int                  height;
	uint32_t             granW;
	uint32_t             granH;
	VkDeviceSize         pageBytes;
	uint32_t             memoryTypeBits;
	VkDeviceSize         alignment;
	VkSamplerAddressMode wrapClampMode;
} vkSparseImage_t;

typedef struct vkSparsePage_s {
	VkDeviceMemory memory;
	qboolean       allocated;
	qboolean       bound;
	int            pageX;
	int            pageY;
} vkSparsePage_t;

typedef struct vkSparsePool_s {
	vkSparseImage_t *owner;
	vkSparsePage_t   pages[VK_SPARSE_MAX_PAGES];
	int              capacity;
	int              allocated;
	VkFence          fence;
} vkSparsePool_t;

qboolean vk_sparse_available( void );

/* Create sparse OPTIMAL 2D image (no memory bound). Fills out + creates view+descriptor. */
qboolean vk_sparse_create_image2d( vkSparseImage_t *out, int width, int height, VkFormat format,
	VkSamplerAddressMode wrapClampMode, const char *debugName );

void vk_sparse_destroy_image( vkSparseImage_t *img, vkSparsePool_t *pool );

qboolean vk_sparse_pool_init( vkSparsePool_t *pool, vkSparseImage_t *owner, int capacity );
void vk_sparse_pool_shutdown( vkSparsePool_t *pool );

/* Bind physical page memory at virtual page (pageX,pageY) in granularity units. Returns slot or -1. */
int vk_sparse_bind_page( vkSparsePool_t *pool, int pageX, int pageY, int preferSlot );

/* Unbind and free memory for a pool slot. */
qboolean vk_sparse_unbind_page( vkSparsePool_t *pool, int slot );

/* Pixel offset of a virtual page (granularity units). */
void vk_sparse_page_pixel_offset( const vkSparseImage_t *img, int pageX, int pageY, int *outX, int *outY );

#endif
