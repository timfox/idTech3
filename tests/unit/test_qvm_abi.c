/*
 * Retail QVM ABI size guards — core structs must not grow without a deliberate
 * CGAME_IMPORT_API_VERSION / layout bump and legacy copy paths in cl_cgame.c.
 */
#include <stdio.h>
#include <stdlib.h>

#include "qcommon/q_shared.h"
#include "bg_public.h"
#include "tr_types.h"

typedef struct {
	int stringOffsets[1024];
	char stringData[16000];
	int dataCount;
} legacyGameState_t;

#define MAX_ENTITIES_IN_SNAPSHOT 256

typedef struct {
	int snapFlags;
	int ping;
	int serverTime;
	byte areamask[MAX_MAP_AREA_BYTES];
	playerState_t ps;
	int numEntities;
	entityState_t entities[MAX_ENTITIES_IN_SNAPSHOT];
	int numServerCommands;
	int serverCommandSequence;
} legacySnapshot_t;

#define ASSERT_EQ(actual, expected, msg) do { \
	if ( (size_t)(actual) != (size_t)(expected) ) { \
		fprintf(stderr, "FAIL: %s (got %zu expected %zu)\n", msg, (size_t)(actual), (size_t)(expected) ); \
		return 1; \
	} \
} while (0)

#define ASSERT_LE(actual, maxval, msg) do { \
	if ( (size_t)(actual) > (size_t)(maxval) ) { \
		fprintf(stderr, "FAIL: %s (got %zu max %zu)\n", msg, (size_t)(actual), (size_t)(maxval) ); \
		return 1; \
	} \
} while (0)

int main(void)
{
	/* Anchors for retail cgame.qvm / ui.qvm (LP64 host, Q3 1.32 field order). */
	ASSERT_EQ( sizeof( playerState_t ), 468, "playerState_t retail size" );
	ASSERT_EQ( sizeof( entityState_t ), 208, "entityState_t retail size" );
	ASSERT_EQ( sizeof( trace_t ), 52, "trace_t retail size" );
	ASSERT_EQ( sizeof( usercmd_t ), 24, "usercmd_t retail size" );
	ASSERT_EQ( sizeof( glconfig_t ), 11324, "glconfig_t retail size" );
	ASSERT_EQ( sizeof( legacyGameState_t ), 20100, "legacyGameState_t retail size" );
	ASSERT_EQ( sizeof( legacySnapshot_t ), 53772, "legacySnapshot_t retail size" );
	ASSERT_LE( MAX_CONFIGSTRINGS, 32768, "MAX_CONFIGSTRINGS engine ceiling" );

	printf("PASS: QVM ABI sizes match retail anchors\n");
	return 0;
}
