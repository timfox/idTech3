#pragma once

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cwchar>

inline void debug_warn(const wchar_t* message)
{
	std::fwprintf(stderr, L"math warning: %ls\n", message ? message : L"(null)");
}

#ifndef ASSERT
#define ASSERT(expr) assert(expr)
#endif

#ifndef ENSURE
#define ENSURE(expr) ASSERT(expr)
#endif
