/*
 * Unit test: reflection source hierarchy fallback order.
 *
 * planar → SSR → ray → probe → sky (vk_reflection_hierarchy.h).
 */
#include <stdio.h>

#define ASSERT(cond, msg) do { \
	if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); return 1; } \
} while (0)

typedef enum {
	REFLECTION_NONE = 0,
	REFLECTION_PLANAR,
	REFLECTION_SSR,
	REFLECTION_RAY,
	REFLECTION_PROBE,
	REFLECTION_SKY,
	REFLECTION_COUNT
} reflection_source_t;

typedef struct {
	int planar;
	int ssr;
	int ray;
	int probe;
	int sky;
} reflection_availability_t;

static reflection_source_t select_reflection_source( const reflection_availability_t *avail )
{
	if ( !avail ) {
		return REFLECTION_NONE;
	}
	if ( avail->planar ) {
		return REFLECTION_PLANAR;
	}
	if ( avail->ssr ) {
		return REFLECTION_SSR;
	}
	if ( avail->ray ) {
		return REFLECTION_RAY;
	}
	if ( avail->probe ) {
		return REFLECTION_PROBE;
	}
	if ( avail->sky ) {
		return REFLECTION_SKY;
	}
	return REFLECTION_NONE;
}

static int source_rank( reflection_source_t src )
{
	switch ( src ) {
	case REFLECTION_PLANAR: return 5;
	case REFLECTION_SSR: return 4;
	case REFLECTION_RAY: return 3;
	case REFLECTION_PROBE: return 2;
	case REFLECTION_SKY: return 1;
	default: return 0;
	}
}

int main( void )
{
	reflection_availability_t all = { 1, 1, 1, 1, 1 };
	reflection_availability_t ssrOnly = { 0, 1, 0, 1, 1 };
	reflection_availability_t skyOnly = { 0, 0, 0, 0, 1 };

	ASSERT( select_reflection_source( &all ) == REFLECTION_PLANAR, "planar highest priority" );
	ASSERT( select_reflection_source( &ssrOnly ) == REFLECTION_SSR, "ssr before probe/sky" );
	ASSERT( select_reflection_source( &skyOnly ) == REFLECTION_SKY, "sky last resort" );
	ASSERT( source_rank( REFLECTION_PLANAR ) > source_rank( REFLECTION_SSR ), "rank ordering" );
	ASSERT( source_rank( REFLECTION_PROBE ) > source_rank( REFLECTION_SKY ), "probe before sky" );

	printf( "unit_reflection_fallback: PASS\n" );
	return 0;
}
