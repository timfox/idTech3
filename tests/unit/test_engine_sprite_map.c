/*
 * Unit test: EngineSpriteMap_Parse (misc_billboard / flipbook / imposter).
 */
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "qcommon/q_shared.h"
#include "qcommon/engine_sprite_map.h"

#define ASSERT(cond, msg) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

#define ASSERT_EQ(a, b, msg) do { \
	if ((int)(a) != (int)(b)) { \
		fprintf(stderr, "FAIL: %s (got %d want %d)\n", msg, (int)(a), (int)(b)); \
		return 1; \
	} \
} while (0)

#define ASSERT_FLOAT(a, b, msg) do { \
	if (fabs((double)(a) - (double)(b)) > 0.001) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

static int test_parse_mixed_props(void)
{
	static const char ents[] =
		"{\n"
		"\"classname\" \"misc_billboard\"\n"
		"\"origin\" \"128 0 32\"\n"
		"\"shader\" \"sprites/tree\"\n"
		"\"scale\" \"64\"\n"
		"}\n"
		"{\n"
		"\"classname\" \"misc_flipbook\"\n"
		"\"origin\" \"0 64 16\"\n"
		"\"model\" \"sprites/fire\"\n"
		"\"cols\" \"4\"\n"
		"\"rows\" \"2\"\n"
		"\"fps\" \"12\"\n"
		"}\n"
		"{\n"
		"\"classname\" \"misc_imposter\"\n"
		"\"origin\" \"-32 0 48\"\n"
		"\"shader\" \"sprites/rock\"\n"
		"\"rotation\" \"45\"\n"
		"}\n"
		"{\n"
		"\"classname\" \"misc_voxel\"\n"
		"\"origin\" \"64 64 16\"\n"
		"\"model\" \"models/vox/demo_crate.vox\"\n"
		"\"scale\" \"2\"\n"
		"\"angle\" \"90\"\n"
		"}\n"
		"{\n"
		"\"classname\" \"info_player_start\"\n"
		"\"origin\" \"0 0 0\"\n"
		"}\n";
	engineSpriteMapList_t list;

	EngineSpriteMap_Parse( ents, &list );
	ASSERT_EQ( list.count, 4, "four sprite props" );
	ASSERT_EQ( list.defs[0].type, ENGINE_SPRITE_BILLBOARD, "billboard type" );
	ASSERT_FLOAT( list.defs[0].origin[0], 128.0f, "billboard origin x" );
	ASSERT_FLOAT( list.defs[0].radius, 64.0f, "billboard scale" );
	ASSERT_EQ( list.defs[1].type, ENGINE_SPRITE_FLIPBOOK, "flipbook type" );
	ASSERT_EQ( list.defs[1].cols, 4, "flipbook cols" );
	ASSERT_EQ( list.defs[1].rows, 2, "flipbook rows" );
	ASSERT_FLOAT( list.defs[1].fps, 12.0f, "flipbook fps" );
	ASSERT_EQ( list.defs[2].type, ENGINE_SPRITE_IMPOSTER, "imposter type" );
	ASSERT_FLOAT( list.defs[2].rotation, 45.0f, "imposter rotation" );
	ASSERT_EQ( list.defs[3].type, ENGINE_SPRITE_VOXEL, "voxel type" );
	ASSERT_FLOAT( list.defs[3].radius, 2.0f, "voxel scale" );
	ASSERT_FLOAT( list.defs[3].rotation, 90.0f, "voxel yaw" );
	ASSERT( strstr( list.defs[3].shader, "demo_crate.vox" ) != NULL, "voxel model path" );
	return 0;
}

static int test_voxel_default_scale( void )
{
	static const char ents[] =
		"{\n"
		"\"classname\" \"misc_voxel\"\n"
		"\"origin\" \"0 0 0\"\n"
		"\"model\" \"models/vox/a.vox\"\n"
		"}\n";
	engineSpriteMapList_t list;

	EngineSpriteMap_Parse( ents, &list );
	ASSERT_EQ( list.count, 1, "one voxel" );
	ASSERT_EQ( list.defs[0].type, ENGINE_SPRITE_VOXEL, "voxel type" );
	ASSERT_FLOAT( list.defs[0].radius, 1.0f, "default voxel scale 1" );
	return 0;
}

int main(void)
{
	if (test_parse_mixed_props() != 0) {
		return 1;
	}
	if (test_voxel_default_scale() != 0) {
		return 1;
	}
	printf("test_engine_sprite_map unit: ok\n");
	return 0;
}
