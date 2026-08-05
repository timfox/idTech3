/*
 * Bounded rectangle allocator for renderer atlases.
 *
 * The allocator is deliberately fixed-capacity: allocation and release never
 * call malloc, which keeps it usable during renderer restart and streaming.
 * Dirty rectangles are tracked separately so uploads can be restricted to
 * changed regions instead of rewriting an entire atlas.
 */
#pragma once

#include "q_shared.h"

#define VK_RECT_ALLOC_MAX_FREE 4096
#define VK_RECT_ALLOC_MAX_ALLOCS 2048
#define VK_RECT_ALLOC_MAX_DIRTY 128

typedef struct vkRect_t {
	int x, y, w, h;
} vkRect_t;

typedef struct vkRectAllocation_s {
	qboolean active;
	uint32_t generation;
	vkRect_t storage;
	vkRect_t content;
} vkRectAllocation_t;

typedef struct vkRectAllocator_s {
	int width, height;
	qboolean initialized;
	vkRect_t freeRects[VK_RECT_ALLOC_MAX_FREE];
	int freeCount;
	vkRectAllocation_t allocations[VK_RECT_ALLOC_MAX_ALLOCS];
	vkRect_t dirty[VK_RECT_ALLOC_MAX_DIRTY];
	int dirtyCount;
	uint32_t generation;
} vkRectAllocator_t;

void vk_rect_allocator_init( vkRectAllocator_t *allocator, int width, int height );
void vk_rect_allocator_reset( vkRectAllocator_t *allocator );
qboolean vk_rect_allocator_alloc( vkRectAllocator_t *allocator, int width, int height,
	int padding, uint32_t *handle, vkRect_t *content );
qboolean vk_rect_allocator_free( vkRectAllocator_t *allocator, uint32_t handle );
qboolean vk_rect_allocator_get( const vkRectAllocator_t *allocator, uint32_t handle,
	vkRect_t *content );

void vk_rect_allocator_mark_dirty( vkRectAllocator_t *allocator, vkRect_t rect );
void vk_rect_allocator_mark_dirty_handle( vkRectAllocator_t *allocator, uint32_t handle );
int vk_rect_allocator_dirty_count( const vkRectAllocator_t *allocator );
qboolean vk_rect_allocator_dirty_at( const vkRectAllocator_t *allocator, int index,
	vkRect_t *rect );
void vk_rect_allocator_clear_dirty( vkRectAllocator_t *allocator );

