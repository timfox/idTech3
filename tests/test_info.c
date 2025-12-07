/*
===========================================================================
Info string helpers tests (Info_ValueForKey, Info_SetValueForKey, Info_RemoveKey)
===========================================================================
*/

#include "test_framework.h"
#include "../src/qcommon/q_shared.h"

// Minimal Com_Printf stub for the test framework
void Com_Printf(const char *fmt, ...) {
	va_list argptr;
	va_start(argptr, fmt);
	vprintf(fmt, argptr);
	va_end(argptr);
}

TEST(info_value_for_key_basic) {
	const char *info = "\\name\\player\\team\\red";
	ASSERT_STR_EQ(Info_ValueForKey(info, "name"), "player");
	ASSERT_STR_EQ(Info_ValueForKey(info, "team"), "red");
	ASSERT_STR_EQ(Info_ValueForKey(info, "missing"), "");
}

TEST(info_set_and_get_roundtrip) {
	char info[MAX_INFO_STRING] = {0};

	ASSERT_TRUE(Info_SetValueForKey(info, "name", "alice"));
	ASSERT_TRUE(Info_SetValueForKey(info, "team", "blue"));

	ASSERT_STR_EQ(Info_ValueForKey(info, "name"), "alice");
	ASSERT_STR_EQ(Info_ValueForKey(info, "team"), "blue");
}

TEST(info_rejects_illegal_chars) {
	char info[MAX_INFO_STRING] = {0};
	// Keys or values containing quotes, semicolons or backslashes should be rejected
	ASSERT_FALSE(Info_SetValueForKey(info, "bad\"key", "ok"));
	ASSERT_FALSE(Info_SetValueForKey(info, "bad;key", "ok"));
	ASSERT_FALSE(Info_SetValueForKey(info, "bad\\key", "ok"));
	ASSERT_FALSE(Info_SetValueForKey(info, "ok", "bad\"val"));
	ASSERT_FALSE(Info_SetValueForKey(info, "ok", "bad;val"));
	ASSERT_FALSE(Info_SetValueForKey(info, "ok", "bad\\val"));
}

TEST(info_remove_key) {
	char info[MAX_INFO_STRING] = {0};
	Info_SetValueForKey(info, "k1", "v1");
	Info_SetValueForKey(info, "k2", "v2");
	Info_SetValueForKey(info, "k3", "v3");

	int removed = Info_RemoveKey(info, "k2");
	ASSERT_TRUE(removed > 0);
	ASSERT_STR_EQ(Info_ValueForKey(info, "k2"), "");
	ASSERT_STR_EQ(Info_ValueForKey(info, "k1"), "v1");
	ASSERT_STR_EQ(Info_ValueForKey(info, "k3"), "v3");
}

int main(void) {
	Com_Printf("Running info string tests...\n\n");

	RUN_TEST(info_value_for_key_basic);
	RUN_TEST(info_set_and_get_roundtrip);
	RUN_TEST(info_rejects_illegal_chars);
	RUN_TEST(info_remove_key);

	PRINT_TEST_SUMMARY();
	return (test_failed > 0) ? 1 : 0;
}


