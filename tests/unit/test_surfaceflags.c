/*
 * Unit tests: surface / content flags (header-only contract)
 */
#include <stdio.h>
#include <stdlib.h>

#include "qcommon/surfaceflags.h"

#define ASSERT(cond, msg) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

int main(void)
{
	ASSERT(CONTENTS_SOLID == 1, "CONTENTS_SOLID");
	ASSERT(CONTENTS_BODY != 0, "CONTENTS_BODY nonzero");
	ASSERT(SURF_SKY == 0x4, "SURF_SKY value");
	ASSERT(CONTENTS_WATER == 32, "CONTENTS_WATER");

	printf("PASS: unit_surfaceflags\n");
	return 0;
}
