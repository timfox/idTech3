/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "test_framework.h"
#include "../src/qcommon/q_shared.h"

// Mock Com_Printf for testing
void Com_Printf(const char *fmt, ...) {
	va_list argptr;
	va_start(argptr, fmt);
	vprintf(fmt, argptr);
	va_end(argptr);
}

TEST(q_strncpyz_basic) {
	char dest[64];
	const char *src = "hello";
	
	Q_strncpyz(dest, src, sizeof(dest));
	ASSERT_STR_EQ(dest, "hello");
}

TEST(q_strncpyz_truncation) {
	char dest[5];
	const char *src = "hello world";
	
	Q_strncpyz(dest, src, sizeof(dest));
	ASSERT_STR_EQ(dest, "hell");
}

TEST(q_strncpyz_empty_string) {
	char dest[64];
	const char *src = "";
	
	Q_strncpyz(dest, src, sizeof(dest));
	ASSERT_STR_EQ(dest, "");
	ASSERT_EQ(dest[0], '\0');
}

TEST(q_stricmp_basic) {
	ASSERT_EQ(Q_stricmp("hello", "HELLO"), 0);
	ASSERT_EQ(Q_stricmp("hello", "world"), -1);
	ASSERT_EQ(Q_stricmp("world", "hello"), 1);
}

TEST(q_streq_basic) {
	ASSERT_TRUE(Q_streq("hello", "hello"));
	ASSERT_FALSE(Q_streq("hello", "world"));
	ASSERT_FALSE(Q_streq("hello", "HELLO"));
}

int main(void) {
	Com_Printf("Running qcommon tests...\n\n");
	
	RUN_TEST(q_strncpyz_basic);
	RUN_TEST(q_strncpyz_truncation);
	RUN_TEST(q_strncpyz_empty_string);
	RUN_TEST(q_stricmp_basic);
	RUN_TEST(q_streq_basic);
	
	PRINT_TEST_SUMMARY();
	
	return (test_failed > 0) ? 1 : 0;
}

