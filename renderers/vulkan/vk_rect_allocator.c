#include "vk_rect_allocator.h"

#include <limits.h>

static qboolean rect_valid( vkRect_t r ) {
	return r.w > 0 && r.h > 0;
}

static qboolean rect_overlaps_or_touches( vkRect_t a, vkRect_t b ) {
	return a.x <= b.x + b.w && a.x + a.w >= b.x &&
		a.y <= b.y + b.h && a.y + a.h >= b.y;
}

static qboolean rect_merge( vkRect_t a, vkRect_t b, vkRect_t *out ) {
	if ( a.y == b.y && a.h == b.h &&
		(a.x + a.w == b.x || b.x + b.w == a.x) ) {
		out->x = MIN( a.x, b.x );
		out->y = a.y;
		out->w = a.w + b.w;
		out->h = a.h;
		return qtrue;
	}
	if ( a.x == b.x && a.w == b.w &&
		(a.y + a.h == b.y || b.y + b.h == a.y) ) {
		out->x = a.x;
		out->y = MIN( a.y, b.y );
		out->w = a.w;
		out->h = a.h + b.h;
		return qtrue;
	}
	return qfalse;
}

static void free_remove( vkRectAllocator_t *a, int index ) {
	if ( index < 0 || index >= a->freeCount ) return;
	a->freeRects[index] = a->freeRects[--a->freeCount];
}

static qboolean free_push( vkRectAllocator_t *a, vkRect_t rect ) {
	if ( !rect_valid( rect ) ) return qtrue;
	if ( a->freeCount >= VK_RECT_ALLOC_MAX_FREE ) return qfalse;
	a->freeRects[a->freeCount++] = rect;
	return qtrue;
}

static void free_coalesce( vkRectAllocator_t *a ) {
	qboolean merged;
	do {
		int i, j;
		merged = qfalse;
		for ( i = 0; i < a->freeCount && !merged; ++i ) {
			for ( j = i + 1; j < a->freeCount; ++j ) {
				vkRect_t combined;
				if ( rect_merge( a->freeRects[i], a->freeRects[j], &combined ) ) {
					a->freeRects[i] = combined;
					free_remove( a, j );
					merged = qtrue;
					break;
				}
			}
		}
	} while ( merged );
}

void vk_rect_allocator_init( vkRectAllocator_t *a, int width, int height ) {
	if ( !a ) return;
	Com_Memset( a, 0, sizeof( *a ) );
	a->width = width;
	a->height = height;
	a->generation = 1;
	a->initialized = (width > 0 && height > 0) ? qtrue : qfalse;
	if ( a->initialized ) free_push( a, (vkRect_t){ 0, 0, width, height } );
}

void vk_rect_allocator_reset( vkRectAllocator_t *a ) {
	if ( !a ) return;
	vk_rect_allocator_init( a, a->width, a->height );
}

qboolean vk_rect_allocator_alloc( vkRectAllocator_t *a, int width, int height,
	int padding, uint32_t *handle, vkRect_t *content ) {
	int i, best = -1, slot = -1;
	int totalW, totalH;
	long long bestWaste = LLONG_MAX;
	if ( !a || !a->initialized || !handle || !content || width <= 0 || height <= 0 ) return qfalse;
	if ( padding < 0 || width > INT_MAX - padding * 2 || height > INT_MAX - padding * 2 ) return qfalse;
	totalW = width + padding * 2;
	totalH = height + padding * 2;
	for ( i = 0; i < VK_RECT_ALLOC_MAX_ALLOCS; ++i ) {
		if ( !a->allocations[i].active ) { slot = i; break; }
	}
	if ( slot < 0 ) return qfalse;
	for ( i = 0; i < a->freeCount; ++i ) {
		vkRect_t r = a->freeRects[i];
		if ( r.w >= totalW && r.h >= totalH ) {
			long long waste = (long long)r.w * r.h - (long long)totalW * totalH;
			if ( waste < bestWaste ) { bestWaste = waste; best = i; }
		}
	}
	if ( best < 0 ) return qfalse;
	{
		vkRect_t source = a->freeRects[best];
		vkRect_t right = { source.x + totalW, source.y, source.w - totalW, source.h };
		vkRect_t bottom = { source.x, source.y + totalH, totalW, source.h - totalH };
		int fragments = (rect_valid( right ) ? 1 : 0) + (rect_valid( bottom ) ? 1 : 0);
		if ( a->freeCount - 1 + fragments > VK_RECT_ALLOC_MAX_FREE ) return qfalse;
		free_remove( a, best );
		(void)free_push( a, right );
		(void)free_push( a, bottom );
		a->allocations[slot].active = qtrue;
		a->allocations[slot].generation = ++a->generation;
		a->allocations[slot].storage = (vkRect_t){ source.x, source.y, totalW, totalH };
		a->allocations[slot].content = (vkRect_t){ source.x + padding, source.y + padding, width, height };
		*handle = (a->allocations[slot].generation << 11) | (uint32_t)slot;
		*content = a->allocations[slot].content;
		return qtrue;
	}
}

static vkRectAllocation_t *get_alloc( vkRectAllocator_t *a, uint32_t handle ) {
	uint32_t slot = handle & ((1u << 11) - 1u);
	if ( !a || slot >= VK_RECT_ALLOC_MAX_ALLOCS || !a->allocations[slot].active ||
		(a->allocations[slot].generation << 11) != (handle & ~((1u << 11) - 1u)) ) return NULL;
	return &a->allocations[slot];
}

qboolean vk_rect_allocator_free( vkRectAllocator_t *a, uint32_t handle ) {
	vkRectAllocation_t *allocation = get_alloc( a, handle );
	if ( !allocation || !free_push( a, allocation->storage ) ) return qfalse;
	vk_rect_allocator_mark_dirty( a, allocation->storage );
	allocation->active = qfalse;
	free_coalesce( a );
	return qtrue;
}

qboolean vk_rect_allocator_get( const vkRectAllocator_t *a, uint32_t handle, vkRect_t *content ) {
	uint32_t slot = handle & ((1u << 11) - 1u);
	if ( !a || !content || slot >= VK_RECT_ALLOC_MAX_ALLOCS || !a->allocations[slot].active ||
		(a->allocations[slot].generation << 11) != (handle & ~((1u << 11) - 1u)) ) return qfalse;
	*content = a->allocations[slot].content;
	return qtrue;
}

void vk_rect_allocator_mark_dirty( vkRectAllocator_t *a, vkRect_t rect ) {
	int i;
	if ( !a || !rect_valid( rect ) ) return;
	if ( rect.x < 0 ) { rect.w += rect.x; rect.x = 0; }
	if ( rect.y < 0 ) { rect.h += rect.y; rect.y = 0; }
	if ( rect.x + rect.w > a->width ) rect.w = a->width - rect.x;
	if ( rect.y + rect.h > a->height ) rect.h = a->height - rect.y;
	if ( !rect_valid( rect ) ) return;
	/* Consume every entry touched by the expanded rectangle. Restarting after
	 * removal handles transitive merges without leaving duplicate dirty work. */
	for ( i = 0; i < a->dirtyCount; ) {
		if ( !rect_overlaps_or_touches( a->dirty[i], rect ) ) {
			++i;
			continue;
		}
		rect.x = MIN( rect.x, a->dirty[i].x );
		rect.y = MIN( rect.y, a->dirty[i].y );
		rect.w = MAX( rect.x + rect.w, a->dirty[i].x + a->dirty[i].w ) - rect.x;
		rect.h = MAX( rect.y + rect.h, a->dirty[i].y + a->dirty[i].h ) - rect.y;
		a->dirty[i] = a->dirty[--a->dirtyCount];
		i = 0;
	}
	if ( a->dirtyCount < VK_RECT_ALLOC_MAX_DIRTY ) {
		a->dirty[a->dirtyCount++] = rect;
	} else {
		/* Bounded overflow policy: one full-atlas upload is safer than dropping a change. */
		a->dirty[0] = (vkRect_t){ 0, 0, a->width, a->height };
		a->dirtyCount = 1;
	}
}

void vk_rect_allocator_mark_dirty_handle( vkRectAllocator_t *a, uint32_t handle ) {
	vkRectAllocation_t *allocation = get_alloc( a, handle );
	if ( allocation ) vk_rect_allocator_mark_dirty( a, allocation->content );
}

int vk_rect_allocator_dirty_count( const vkRectAllocator_t *a ) { return a ? a->dirtyCount : 0; }

qboolean vk_rect_allocator_dirty_at( const vkRectAllocator_t *a, int index, vkRect_t *rect ) {
	if ( !a || !rect || index < 0 || index >= a->dirtyCount ) return qfalse;
	*rect = a->dirty[index];
	return qtrue;
}

void vk_rect_allocator_clear_dirty( vkRectAllocator_t *a ) { if ( a ) a->dirtyCount = 0; }
