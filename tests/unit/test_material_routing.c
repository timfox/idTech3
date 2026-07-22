/*
 * Unit test: deferred vs Forward+ material routing (Clustered Hybrid M1).
 *
 * Pure-C duplicate of R_SelectSurfaceRenderPath rules from docs/RENDERER_PATH_OWNERSHIP.md
 * and renderers/vulkan/vk_render_path.c.
 */
#include <stdio.h>

#define ASSERT(cond, msg) do { \
	if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); return 1; } \
} while (0)

typedef enum {
	RENDER_PATH_LEGACY_FORWARD = 1,
	RENDER_PATH_DEFERRED_OPAQUE,
	RENDER_PATH_FORWARD_PLUS_OPAQUE,
	RENDER_PATH_FORWARD_PLUS_TRANSPARENT,
	RENDER_PATH_FORWARD_PLUS_WEAPON,
	RENDER_PATH_OIT,
	RENDER_PATH_SKY,
	RENDER_PATH_UI
} render_path_t;

typedef struct {
	int isSky;
	int isTransparent;
	int isComplexOpaque;
	int isRefractive;
} shader_flags_t;

static render_path_t select_surface_render_path(
	int renderMode,
	int deferredReady,
	int deferredSplit,
	int forwardPlus,
	int oitEnabled,
	int viewClassUi,
	shader_flags_t sh )
{
	if ( viewClassUi ) {
		return RENDER_PATH_UI;
	}
	if ( sh.isSky ) {
		return RENDER_PATH_SKY;
	}
	if ( sh.isTransparent ) {
		if ( oitEnabled && deferredSplit ) {
			return RENDER_PATH_OIT;
		}
		return RENDER_PATH_FORWARD_PLUS_TRANSPARENT;
	}
	if ( renderMode == 0 ) {
		return RENDER_PATH_LEGACY_FORWARD;
	}
	if ( sh.isComplexOpaque || sh.isRefractive ) {
		return RENDER_PATH_FORWARD_PLUS_OPAQUE;
	}
	if ( ( renderMode == 1 || renderMode == 3 || renderMode == 4 ) &&
		deferredReady && deferredSplit ) {
		return RENDER_PATH_DEFERRED_OPAQUE;
	}
	if ( forwardPlus ) {
		return RENDER_PATH_FORWARD_PLUS_OPAQUE;
	}
	return RENDER_PATH_LEGACY_FORWARD;
}

int main( void )
{
	shader_flags_t opaque = { 0, 0, 0, 0 };
	shader_flags_t glass = { 0, 1, 0, 1 };
	shader_flags_t complex = { 0, 0, 1, 0 };

	ASSERT( select_surface_render_path( 3, 1, 1, 1, 0, 0, opaque ) ==
		RENDER_PATH_DEFERRED_OPAQUE, "mode3 standard opaque → deferred" );

	ASSERT( select_surface_render_path( 3, 1, 1, 1, 0, 0, complex ) ==
		RENDER_PATH_FORWARD_PLUS_OPAQUE, "complex opaque → Forward+ fallback" );

	ASSERT( select_surface_render_path( 2, 1, 1, 1, 0, 0, opaque ) ==
		RENDER_PATH_FORWARD_PLUS_OPAQUE, "mode2 keeps Forward+ on opaque" );

	ASSERT( select_surface_render_path( 3, 0, 1, 1, 0, 0, opaque ) ==
		RENDER_PATH_FORWARD_PLUS_OPAQUE, "deferred not ready fails open" );

	ASSERT( select_surface_render_path( 3, 1, 1, 1, 1, 0, glass ) ==
		RENDER_PATH_OIT, "transparent + oit + split" );

	ASSERT( select_surface_render_path( 3, 1, 1, 1, 0, 1, opaque ) ==
		RENDER_PATH_UI, "ui view class" );

	printf( "unit_material_routing: PASS\n" );
	return 0;
}
