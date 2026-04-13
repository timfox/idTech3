/*
 * Unit tests: info-string helpers in q_shared.c (Info_*).
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

static int test_value_for_key_basic(void)
{
	const char *s = "\\name\\tim\\rate\\25000";

	ASSERT_STREQ(Info_ValueForKey(s, "name"), "tim", "Info_ValueForKey name");
	ASSERT_STREQ(Info_ValueForKey(s, "rate"), "25000", "Info_ValueForKey rate");
	ASSERT_STREQ(Info_ValueForKey(s, "missing"), "", "Info_ValueForKey missing");
	return 0;
}

static int test_value_for_key_edge_args(void)
{
	const char *s = "\\k\\v";

	ASSERT_STREQ(Info_ValueForKey(NULL, "k"), "", "NULL s");
	ASSERT_STREQ(Info_ValueForKey(s, NULL), "", "NULL key");
	ASSERT_STREQ(Info_ValueForKey(s, ""), "", "empty key");
	return 0;
}

static int test_value_for_key_case_insensitive(void)
{
	const char *s = "\\NAME\\upper\\mix\\ok";

	ASSERT_STREQ(Info_ValueForKey(s, "name"), "upper", "key case fold");
	ASSERT_STREQ(Info_ValueForKey(s, "MIX"), "ok", "key case fold 2");
	return 0;
}

static int test_value_for_key_no_leading_slash(void)
{
	/* Parser tolerates missing leading backslash */
	const char *s = "solo\\only";

	ASSERT_STREQ(Info_ValueForKey(s, "solo"), "only", "no leading slash");
	return 0;
}

static int test_value_for_key_dual_buffer(void)
{
	const char *s = "\\a\\1\\b\\2";
	const char *x = Info_ValueForKey(s, "a");
	const char *y = Info_ValueForKey(s, "b");

	ASSERT_STREQ(x, "1", "dual buf a");
	ASSERT_STREQ(y, "2", "dual buf b");
	return 0;
}

static int test_remove_key(void)
{
	char buf[128];

	Q_strncpyz(buf, "\\name\\tim\\rate\\25000\\extra\\x", sizeof(buf));
	ASSERT_EQ(Info_RemoveKey(buf, "rate"), 11, "RemoveKey rate count");
	ASSERT_STREQ(Info_ValueForKey(buf, "name"), "tim", "neighbor after remove");
	ASSERT_STREQ(Info_ValueForKey(buf, "rate"), "", "rate gone");
	ASSERT_STREQ(Info_ValueForKey(buf, "extra"), "x", "tail preserved");

	Q_strncpyz(buf, "\\only\\1", sizeof(buf));
	ASSERT_EQ(Info_RemoveKey(buf, "only"), (int)strlen("\\only\\1"), "remove sole pair");
	ASSERT_STREQ(buf, "", "empty after sole remove");
	return 0;
}

static int test_remove_key_missing(void)
{
	char buf[128];
	const char *orig = "\\a\\1\\b\\2";

	Q_strncpyz(buf, orig, sizeof(buf));
	ASSERT_EQ(Info_RemoveKey(buf, "nope"), 0, "remove missing returns 0");
	ASSERT_STREQ(buf, orig, "unchanged when key missing");
	return 0;
}

static int test_set_value_for_key_overwrite(void)
{
	char buf[MAX_INFO_STRING];

	buf[0] = '\0';
	ASSERT(Info_SetValueForKey(buf, "rate", "1"), "set rate 1");
	ASSERT_STREQ(Info_ValueForKey(buf, "rate"), "1", "rate 1");
	ASSERT(Info_SetValueForKey(buf, "rate", "25000"), "overwrite rate");
	ASSERT_STREQ(Info_ValueForKey(buf, "rate"), "25000", "rate overwritten");
	ASSERT(Info_SetValueForKey(buf, "rate", "99"), "second overwrite rate");
	ASSERT_STREQ(Info_ValueForKey(buf, "rate"), "99", "rate third value");
	ASSERT_STREQ(Info_ValueForKey(buf, "name"), "", "no stray name");
	return 0;
}

static int test_set_value_for_key_multi(void)
{
	char buf[MAX_INFO_STRING];

	buf[0] = '\0';
	ASSERT(Info_SetValueForKey(buf, "name", "tim"), "set name");
	ASSERT(Info_SetValueForKey(buf, "rate", "25000"), "set rate");
	ASSERT_STREQ(Info_ValueForKey(buf, "name"), "tim", "multi name");
	ASSERT_STREQ(Info_ValueForKey(buf, "rate"), "25000", "multi rate");
	return 0;
}

static int test_set_value_for_key_clear_value(void)
{
	char buf[MAX_INFO_STRING];

	Q_strncpyz(buf, "\\k\\v", sizeof(buf));
	ASSERT(Info_SetValueForKey(buf, "k", ""), "empty value remove");
	ASSERT_STREQ(Info_ValueForKey(buf, "k"), "", "key cleared");
	ASSERT_STREQ(buf, "", "buf empty after clear");

	Q_strncpyz(buf, "\\a\\1\\b\\2", sizeof(buf));
	ASSERT(Info_SetValueForKey(buf, "a", NULL), "NULL value remove");
	ASSERT_STREQ(Info_ValueForKey(buf, "a"), "", "NULL clears");
	ASSERT_STREQ(Info_ValueForKey(buf, "b"), "2", "neighbor after NULL clear");
	return 0;
}

static int test_set_value_for_key_invalid(void)
{
	char buf[MAX_INFO_STRING];

	buf[0] = '\0';
	ASSERT(!Info_SetValueForKey(buf, "", "x"), "reject empty key");
	ASSERT(!Info_SetValueForKey(buf, NULL, "x"), "reject NULL key");
	ASSERT(!Info_SetValueForKey(buf, "bad\\key", "1"), "reject backslash in key");
	ASSERT(!Info_SetValueForKey(buf, "k", "v;bad"), "reject semicolon in value");
	ASSERT(!Info_SetValueForKey(buf, "k", "q\"bad"), "reject quote in value");
	ASSERT(!Info_SetValueForKey(buf, "k", "x\\y"), "reject backslash in value");
	ASSERT(!Info_SetValueForKey(buf, "bad\"k", "1"), "reject quote in key");
	ASSERT(!Info_ValidateKeyValue("\""), "kv reject lone quote");
	ASSERT_STREQ(buf, "", "buf still empty after rejects");
	return 0;
}

static int test_set_value_for_key_s_len_cap(void)
{
	char buf[32];

	memset(buf, 'x', sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';
	/* strlen is 31; slen 31 => len1 >= slen, cannot append */
	ASSERT(!Info_SetValueForKey_s(buf, 31, "a", "b"), "reject when len >= slen");
	return 0;
}

static int test_set_value_for_key_s_big_buffer(void)
{
	char buf[BIG_INFO_STRING];

	buf[0] = '\0';
	ASSERT(Info_SetValueForKey_s(buf, BIG_INFO_STRING, "longmod", "ok"),
	       "BIG_INFO_STRING slen accepts pair");
	ASSERT_STREQ(Info_ValueForKey(buf, "longmod"), "ok", "big buf lookup");
	return 0;
}

static int test_validate(void)
{
	ASSERT(Info_Validate("plain"), "validate plain");
	ASSERT(Info_Validate("a\\b"), "validate backslash allowed");
	ASSERT(!Info_Validate("say \"hi\""), "reject quote");
	ASSERT(!Info_Validate("a;b"), "reject semicolon");
	ASSERT(!Info_Validate("\""), "validate reject quote");

	ASSERT(Info_ValidateKeyValue("ok_key"), "kv ok");
	ASSERT(!Info_ValidateKeyValue("bad\\k"), "kv reject backslash");
	ASSERT(!Info_ValidateKeyValue("x;y"), "kv reject semicolon");
	return 0;
}

static int test_tokenize_and_value_token(void)
{
	const char *s = "\\name\\tim\\rate\\25000";

	Info_Tokenize(s);
	ASSERT_STREQ(Info_ValueForKeyToken("name"), "tim", "token name");
	ASSERT_STREQ(Info_ValueForKeyToken("rate"), "25000", "token rate");
	ASSERT_STREQ(Info_ValueForKeyToken("missing"), "", "token missing");

	/* Second tokenize replaces table */
	Info_Tokenize("\\only\\x");
	ASSERT_STREQ(Info_ValueForKeyToken("only"), "x", "tokenize replace only");
	ASSERT_STREQ(Info_ValueForKeyToken("name"), "", "tokenize drops prior keys");
	return 0;
}

static int test_next_pair_iterate(void)
{
	const char *s = "\\name\\tim\\rate\\25000";
	char k[MAX_INFO_KEY];
	char v[MAX_INFO_VALUE];
	const char *p = s;
	int n = 0;

	while (*p) {
		p = Info_NextPair(p, k, v);
		if (k[0] == '\0')
			break;
		n++;
		if (strcmp(k, "name") == 0)
			ASSERT_STREQ(v, "tim", "NextPair name");
		else if (strcmp(k, "rate") == 0)
			ASSERT_STREQ(v, "25000", "NextPair rate");
	}
	ASSERT_EQ(n, 2, "NextPair count");
	return 0;
}

static int test_malformed_trailing_separator(void)
{
	/* Trailing \\ after last pair: parser returns empty for next key */
	const char *s = "\\a\\1\\";

	ASSERT_STREQ(Info_ValueForKey(s, "a"), "1", "trailing slash still finds a");
	ASSERT_STREQ(Info_ValueForKey(s, "orphan"), "", "no orphan key");
	return 0;
}

static int test_empty_infostring(void)
{
	char empty[1] = "";

	ASSERT_STREQ(Info_ValueForKey("", "x"), "", "empty string");
	ASSERT_EQ(Info_RemoveKey(empty, "x"), 0, "remove from empty writable buf");
	{
		char z[4] = "";
		ASSERT_EQ(Info_RemoveKey(z, "a"), 0, "remove missing empty buf");
	}
	return 0;
}

int main(void)
{
	if (test_value_for_key_basic()) return 1;
	if (test_value_for_key_edge_args()) return 1;
	if (test_value_for_key_case_insensitive()) return 1;
	if (test_value_for_key_no_leading_slash()) return 1;
	if (test_value_for_key_dual_buffer()) return 1;
	if (test_remove_key()) return 1;
	if (test_remove_key_missing()) return 1;
	if (test_set_value_for_key_overwrite()) return 1;
	if (test_set_value_for_key_multi()) return 1;
	if (test_set_value_for_key_clear_value()) return 1;
	if (test_set_value_for_key_invalid()) return 1;
	if (test_set_value_for_key_s_len_cap()) return 1;
	if (test_set_value_for_key_s_big_buffer()) return 1;
	if (test_validate()) return 1;
	if (test_tokenize_and_value_token()) return 1;
	if (test_next_pair_iterate()) return 1;
	if (test_malformed_trailing_separator()) return 1;
	if (test_empty_infostring()) return 1;
	return 0;
}
