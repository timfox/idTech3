/*
 * Unit test: EngineDecalMap_Parse (misc_decal).
 */
#include <stdio.h>
#include <string.h>

#include "qcommon/q_shared.h"
#include "qcommon/engine_decal_map.h"

#define ASSERT(cond, msg) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

int main(void) {
	static const char ents[] =
		"{\n"
		"\"classname\" \"misc_decal\"\n"
		"\"origin\" \"0 0 64\"\n"
		"\"shader\" \"textures/decal/blood\"\n"
		"\"scale\" \"48\"\n"
		"}\n";
	engineDecalMapList_t list;

	EngineDecalMap_Parse( ents, &list );
	ASSERT( list.count == 1, "one misc_decal" );
	ASSERT( !strcmp( list.defs[0].shader, "textures/decal/blood" ), "shader path" );
	ASSERT( list.defs[0].radius > 40.0f, "radius from scale" );
	printf("PASS: engine_decal_map\n");
	return 0;
}
