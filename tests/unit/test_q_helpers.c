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
	if (test_com_split()) return 1;
	if (test_hash_value()) return 1;
	if (test_hash_color()) return 1;
	if (test_skip_charset_tokens()) return 1;

	printf("PASS: unit_qhelpers\n");
	return 0;
}
