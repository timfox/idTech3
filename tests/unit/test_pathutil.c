/*
 * Unit tests: COM_* path / extension helpers (q_shared.c).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qcommon/q_shared.h"

#define ASSERT(cond, msg) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

#define ASSERT_STREQ(a, b, msg) do { \
	if (strcmp((a), (b)) != 0) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

int main(void)
{
	char path[] = "/foo/bar/quake.map";
	ASSERT_STREQ(COM_SkipPath(path), "quake.map", "COM_SkipPath");

	ASSERT_STREQ(COM_GetExtension("a.bsp"), "bsp", "COM_GetExtension");
	ASSERT_STREQ(COM_GetExtension("noext"), "", "COM_GetExtension none");
	ASSERT_STREQ(COM_GetExtension("/dir/.hidden"), "hidden", "COM_GetExtension dotfile");
	ASSERT_STREQ(COM_GetExtension("file.tar.gz"), "gz", "COM_GetExtension last dot");

	char buf[128];
	COM_StripExtension("maps/q3dm1.bsp", buf, sizeof(buf));
	ASSERT_STREQ(buf, "maps/q3dm1", "COM_StripExtension");

	COM_StripExtension("archive.tar.gz", buf, sizeof(buf));
	ASSERT_STREQ(buf, "archive.tar", "COM_StripExtension multi-dot");

	ASSERT(COM_CompareExtension("x.pk3", ".pk3"), "COM_CompareExtension");
	ASSERT(COM_CompareExtension("X.PK3", ".pk3"), "COM_CompareExtension case");
	ASSERT(!COM_CompareExtension("x.bsp", ".pk3"), "COM_CompareExtension mismatch");

	char de[64];
	strcpy(de, "maps/foo");
	COM_DefaultExtension(de, sizeof(de), ".bsp");
	ASSERT_STREQ(de, "maps/foo.bsp", "COM_DefaultExtension");

	strcpy(de, "maps/foo.bsp");
	COM_DefaultExtension(de, sizeof(de), ".pk3");
	ASSERT_STREQ(de, "maps/foo.bsp", "COM_DefaultExtension no double");

	strcpy(de, "trail/");
	COM_DefaultExtension(de, sizeof(de), ".bsp");
	ASSERT_STREQ(de, "trail/.bsp", "COM_DefaultExtension trailing slash");

	printf("PASS: unit_pathutil\n");
	return 0;
}
