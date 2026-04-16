/*
 * Unit tests: crc32_buffer (q_shared.c) - deterministic CRC.
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

#define ASSERT_EQ_U(a, b, msg) do { \
	if ((a) != (b)) { \
		fprintf(stderr, "FAIL: %s (got %08x want %08x)\n", msg, (unsigned)(a), (unsigned)(b)); \
		return 1; \
	} \
} while (0)

int main(void)
{
	/* zlib polynomial 0xEDB88320 - standard test vector */
	const unsigned int crc123456789 = 0xCBF43926U;

	ASSERT_EQ_U(crc32_buffer(NULL, 0), 0U, "crc32 empty NULL");
	ASSERT_EQ_U(crc32_buffer((const byte *)"", 0), 0U, "crc32 empty len0");

	const byte one = 'a';
	unsigned c1 = crc32_buffer(&one, 1);
	ASSERT(c1 != 0U, "crc32 single byte nonzero");

	const byte nine[] = { '1','2','3','4','5','6','7','8','9' };
	unsigned c9 = crc32_buffer(nine, sizeof(nine));
	ASSERT_EQ_U(c9, crc123456789, "crc32 123456789 vector");

	unsigned c9b = crc32_buffer(nine, sizeof(nine));
	ASSERT_EQ_U(c9, c9b, "crc32 deterministic");

	const byte bin[] = { 0x00, 0xff, 0x80, 0x01 };
	unsigned cb = crc32_buffer(bin, sizeof(bin));
	ASSERT(cb != 0U, "crc32 binary nonzero");

	printf("PASS: unit_crc\n");
	return 0;
}
