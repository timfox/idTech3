#include "vk_rect_allocator.h"

#define CHECK(condition) do { if ( !(condition) ) return 1; } while ( 0 )

int main( void ) {
	vkRectAllocator_t allocator;
	vkRect_t rect;
	uint32_t first = 0, second = 0;

	vk_rect_allocator_init( &allocator, 64, 64 );
	CHECK( vk_rect_allocator_alloc( &allocator, 16, 16, 2, &first, &rect ) );
	CHECK( rect.x == 2 && rect.y == 2 && rect.w == 16 && rect.h == 16 );
	vk_rect_allocator_mark_dirty_handle( &allocator, first );
	CHECK( vk_rect_allocator_dirty_count( &allocator ) == 1 );
	vk_rect_allocator_mark_dirty( &allocator, (vkRect_t){ rect.x + 8, rect.y + 8, 8, 8 } );
	CHECK( vk_rect_allocator_dirty_count( &allocator ) == 1 );

	CHECK( vk_rect_allocator_alloc( &allocator, 8, 8, 0, &second, &rect ) );
	CHECK( vk_rect_allocator_free( &allocator, first ) );
	CHECK( !vk_rect_allocator_get( &allocator, first, &rect ) );
	CHECK( vk_rect_allocator_free( &allocator, second ) );
	CHECK( vk_rect_allocator_alloc( &allocator, 64, 64, 0, &first, &rect ) );
	CHECK( rect.x == 0 && rect.y == 0 && rect.w == 64 && rect.h == 64 );

	return 0;
}
