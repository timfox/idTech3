/*
 * Unit test: q_shared macros and constants
 * Minimal test with no engine dependencies.
 * Run: ctest -R unit_macros
 */
#include <stdio.h>
#include <stdlib.h>

/* Include only the parts we need - avoid full engine init */
#include "qcommon/q_shared.h"

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s\n", msg); \
        return 1; \
    } \
} while (0)

int main(void)
{
    /* PAD macro: align base to alignment */
    ASSERT(PAD(0, 4) == 0, "PAD(0,4)");
    ASSERT(PAD(1, 4) == 4, "PAD(1,4)");
    ASSERT(PAD(4, 4) == 4, "PAD(4,4)");
    ASSERT(PAD(5, 4) == 8, "PAD(5,4)");
    ASSERT(PAD(7, 8) == 8, "PAD(7,8)");
    ASSERT(PAD(8, 8) == 8, "PAD(8,8)");
    ASSERT(PAD(9, 8) == 16, "PAD(9,8)");

    /* PADLEN: padding bytes added */
    ASSERT(PADLEN(0, 4) == 0, "PADLEN(0,4)");
    ASSERT(PADLEN(1, 4) == 3, "PADLEN(1,4)");
    ASSERT(PADLEN(4, 4) == 0, "PADLEN(4,4)");

    /* Constants */
    ASSERT(MAX_QPATH == 64, "MAX_QPATH");
    ASSERT(MAX_STRING_CHARS == 1024, "MAX_STRING_CHARS");

    printf("PASS: unit_macros\n");
    return 0;
}
