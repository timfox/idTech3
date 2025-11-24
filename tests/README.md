# Unit Tests for id Tech 3

This directory contains unit tests for the id Tech 3 engine.

## Building Tests

To build tests, enable the `BUILD_TESTS` CMake option:

```bash
cd build
cmake .. -DBUILD_TESTS=ON
make
```

## Running Tests

After building, run tests with:

```bash
./tests/test_qcommon
```

## Test Structure

Tests are organized by module:
- `test_qcommon.c` - Tests for qcommon module (string functions, utilities)
- `test_qmath.c` - Tests for math functions
- `test_memory.c` - Tests for memory allocators

## Writing Tests

Use the test framework macros:

```c
#include "test_framework.h"

TEST(test_example) {
    ASSERT_EQ(1 + 1, 2);
    ASSERT_STR_EQ("hello", "hello");
    ASSERT_NOT_NULL(ptr);
}
```

## Test Framework Macros

- `TEST(name)` - Define a test function
- `ASSERT_EQ(a, b)` - Assert two values are equal
- `ASSERT_NE(a, b)` - Assert two values are not equal
- `ASSERT_STR_EQ(a, b)` - Assert two strings are equal
- `ASSERT_NOT_NULL(ptr)` - Assert pointer is not NULL
- `ASSERT_NULL(ptr)` - Assert pointer is NULL
- `ASSERT_TRUE(condition)` - Assert condition is true
- `ASSERT_FALSE(condition)` - Assert condition is false

