/*
 * Unit tests: self-contained helpers in q_shared.c (linked with stub Com_* / Q_atof from q_math).
 */
#include <math.h>
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

#define ASSERT_EQ(a, b, msg) do { \
	if ((a) != (b)) { \
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

#define ASSERT_NEAR(a, b, eps, msg) do { \
	if (fabsf((a) - (b)) > (eps)) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

static int test_com_clamp(void)
{
	ASSERT_NEAR(Com_Clamp(0.0f, 10.0f, 5.0f), 5.0f, 0.0001f, "Com_Clamp mid");
	ASSERT_NEAR(Com_Clamp(0.0f, 10.0f, -1.0f), 0.0f, 0.0001f, "Com_Clamp low");
	ASSERT_NEAR(Com_Clamp(0.0f, 10.0f, 99.0f), 10.0f, 0.0001f, "Com_Clamp high");
	return 0;
}

static int test_hex_str(void)
{
	ASSERT_EQ(Com_HexStrToInt(NULL), -1, "Com_HexStrToInt NULL");
	ASSERT_EQ(Com_HexStrToInt("0x"), -1, "Com_HexStrToInt bare 0x");
	ASSERT_EQ(Com_HexStrToInt("0xff"), 255, "Com_HexStrToInt 0xff");
	ASSERT_EQ(Com_HexStrToInt("0x10"), 16, "Com_HexStrToInt 0x10");
	ASSERT_EQ(Com_HexStrToInt("0xG"), -1, "Com_HexStrToInt bad digit");
	return 0;
}

static int test_crc32(void)
{
	const byte data[] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
	unsigned a = crc32_buffer(data, sizeof(data));
	unsigned b = crc32_buffer(data, sizeof(data));
	ASSERT_EQ(a, b, "crc32_buffer deterministic");
	ASSERT(a != 0U, "crc32_buffer nonzero");
	return 0;
}

static int test_com_path(void)
{
	char path[] = "/foo/bar/quake.map";
	ASSERT_STREQ(COM_SkipPath(path), "quake.map", "COM_SkipPath");

	ASSERT_STREQ(COM_GetExtension("a.bsp"), "bsp", "COM_GetExtension");
	ASSERT_STREQ(COM_GetExtension("noext"), "", "COM_GetExtension none");
	ASSERT_STREQ(COM_GetExtension("/dir/.hidden"), "hidden", "COM_GetExtension dotfile");

	char buf[64];
	COM_StripExtension("maps/q3dm1.bsp", buf, sizeof(buf));
	ASSERT_STREQ(buf, "maps/q3dm1", "COM_StripExtension");

	ASSERT(COM_CompareExtension("x.pk3", ".pk3"), "COM_CompareExtension");
	ASSERT(!COM_CompareExtension("x.bsp", ".pk3"), "COM_CompareExtension mismatch");

	char de[32];
	strcpy(de, "maps/foo");
	COM_DefaultExtension(de, sizeof(de), ".bsp");
	ASSERT_STREQ(de, "maps/foo.bsp", "COM_DefaultExtension");

	return 0;
}

static int test_com_split(void)
{
	char line[] = "alpha beta gamma";
	char *toks[4];
	int n = Com_Split(line, toks, 4, ' ');
	ASSERT_EQ(n, 3, "Com_Split count");
	ASSERT_STREQ(toks[0], "alpha", "Com_Split 0");
	ASSERT_STREQ(toks[1], "beta", "Com_Split 1");
	ASSERT_STREQ(toks[2], "gamma", "Com_Split 2");
	return 0;
}

static int test_hash_value(void)
{
	unsigned long h1 = Com_GenerateHashValue("maps/foo.bsp", 1024);
	unsigned long h2 = Com_GenerateHashValue("maps/FOO.BSP", 1024);
	ASSERT_EQ(h1, h2, "Com_GenerateHashValue case-insensitive path");
	return 0;
}

static int test_hash_color(void)
{
	byte rgb[3];
	ASSERT(Com_GetHashColor("#f00", rgb), "Com_GetHashColor #rgb");
	ASSERT_EQ(rgb[0], 255, "Com_GetHashColor r");
	ASSERT_EQ(rgb[1], 0, "Com_GetHashColor g");
	ASSERT_EQ(rgb[2], 0, "Com_GetHashColor b");

	ASSERT(Com_GetHashColor("#aabbcc", rgb), "Com_GetHashColor #rrggbb");
	ASSERT_EQ(rgb[0], 0xaa, "Com_GetHashColor rr");
	ASSERT_EQ(rgb[1], 0xbb, "Com_GetHashColor gg");
	ASSERT_EQ(rgb[2], 0xcc, "Com_GetHashColor bb");

	ASSERT(!Com_GetHashColor("nohash", rgb), "Com_GetHashColor reject");
	ASSERT(!Com_GetHashColor("#gg", rgb), "Com_GetHashColor bad hex");
	return 0;
}

static int test_byte_swap(void)
{
	short s = 0x0102;
	short t = ShortSwap(s);
	unsigned char *pb = (unsigned char *)&t;
	ASSERT(pb[0] == 0x01 && pb[1] == 0x02, "ShortSwap");

	int L = 0x01020304;
	int Ls = LongSwap(L);
	unsigned char *pl = (unsigned char *)&Ls;
	/* Result is 0x04030201; on little-endian first stored byte is low byte 0x01 */
	ASSERT(pl[0] == 0x01 && pl[1] == 0x02 && pl[2] == 0x03 && pl[3] == 0x04, "LongSwap");

	short buf[2] = { 0x1122, 0 };
	CopyShortSwap(&buf[1], &buf[0]);
	ASSERT((unsigned short)buf[1] == 0x2211, "CopyShortSwap");

	int Lb[2] = { 0x11223344, 0 };
	CopyLongSwap(&Lb[1], &Lb[0]);
	pl = (unsigned char *)&Lb[1];
	ASSERT(pl[0] == 0x11 && pl[1] == 0x22 && pl[2] == 0x33 && pl[3] == 0x44, "CopyLongSwap");
	return 0;
}

static int test_skip_charset_tokens(void)
{
	const char *s = Com_SkipCharset("   hello", " ");
	ASSERT_STREQ(s, "hello", "Com_SkipCharset spaces");

	const char *t = "a,b,,c";
	const char *p = Com_SkipTokens(t, 2, ",");
	ASSERT_STREQ(p, "c", "Com_SkipTokens after two separators");
	return 0;
}

int main(void)
{
	if (test_com_clamp()) return 1;
	if (test_hex_str()) return 1;
	if (test_crc32()) return 1;
	if (test_com_path()) return 1;
	if (test_com_split()) return 1;
	if (test_hash_value()) return 1;
	if (test_hash_color()) return 1;
	if (test_byte_swap()) return 1;
	if (test_skip_charset_tokens()) return 1;

	printf("PASS: unit_qhelpers\n");
	return 0;
}
