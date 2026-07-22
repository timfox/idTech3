/*
 * Unit test: GPU scene generation bump + slot reuse temporal contract.
 *
 * Mirrors vk_gpu_scene.c lifecycle: world load / vid_restart bump generation;
 * stale generation rejects cull; slot reuse clears visibleAge and prevTransform.
 */
#include <stdio.h>
#include <string.h>

#define ASSERT(cond, msg) do { \
	if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); return 1; } \
} while (0)

typedef struct {
	unsigned handle;
	unsigned generation;
	unsigned visibleAge;
	float transform[12];
	float prevTransform[12];
} gpu_scene_instance_t;

static unsigned s_generation = 1;
static gpu_scene_instance_t s_instances[8];
static unsigned s_instanceCount;

static void gpu_scene_world_load( void )
{
	s_generation++;
	s_instanceCount = 0;
}

static void gpu_scene_vid_restart( void )
{
	s_generation++;
}

static unsigned gpu_scene_register_instance( unsigned handle )
{
	gpu_scene_instance_t *inst;

	if ( s_instanceCount >= 8u ) {
		return 0;
	}
	inst = &s_instances[s_instanceCount++];
	memset( inst, 0, sizeof( *inst ) );
	inst->handle = handle;
	inst->generation = s_generation;
	inst->transform[0] = 1.0f;
	inst->prevTransform[0] = 1.0f;
	return handle;
}

static gpu_scene_instance_t *gpu_scene_find( unsigned handle )
{
	unsigned i;

	for ( i = 0; i < s_instanceCount; i++ ) {
		if ( s_instances[i].handle == handle ) {
			return &s_instances[i];
		}
	}
	return NULL;
}

static void gpu_scene_reuse_slot( unsigned handle )
{
	gpu_scene_instance_t *inst = gpu_scene_find( handle );

	if ( !inst ) {
		return;
	}
	inst->visibleAge = 0;
	inst->prevTransform[0] = inst->transform[0];
	inst->prevTransform[1] = 0.0f;
}

static int gpu_scene_cull_visible( unsigned handle )
{
	gpu_scene_instance_t *inst = gpu_scene_find( handle );

	if ( !inst || inst->generation != s_generation ) {
		return 0;
	}
	inst->visibleAge++;
	return 1;
}

int main( void )
{
	unsigned h;

	s_generation = 1;
	s_instanceCount = 0;
	h = gpu_scene_register_instance( 42u );
	ASSERT( h == 42u, "register handle" );
	ASSERT( gpu_scene_cull_visible( 42u ), "first gen visible" );
	ASSERT( s_instances[0].visibleAge == 1u, "visibleAge increments" );

	gpu_scene_vid_restart();
	ASSERT( !gpu_scene_cull_visible( 42u ), "stale generation rejected after vid_restart" );

	gpu_scene_world_load();
	h = gpu_scene_register_instance( 42u );
	ASSERT( h == 42u, "slot reuse same handle" );
	ASSERT( s_instances[0].generation == s_generation, "new generation stamped" );
	ASSERT( s_instances[0].visibleAge == 0u, "reuse clears visibleAge" );
	ASSERT( s_instances[0].prevTransform[1] == 0.0f, "reuse resets prev temporal delta" );

	gpu_scene_reuse_slot( 42u );
	ASSERT( s_instances[0].visibleAge == 0u, "explicit reuse clears visibleAge" );

	printf( "unit_gpu_scene_generation: PASS\n" );
	return 0;
}
