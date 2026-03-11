#ifndef Q_COMPAT_H
#define Q_COMPAT_H

#define Q_STATIC_ASSERT_CONCAT_IMPL(a, b) a##b
#define Q_STATIC_ASSERT_CONCAT(a, b) Q_STATIC_ASSERT_CONCAT_IMPL(a, b)

/* Only define _Static_assert for C. In C++, static_assert is a keyword and our macro
 * would break C++ standard library headers (e.g. libc++ on macOS) when expressions
 * contain commas. */
#if !defined(__cplusplus) && !defined(_Static_assert) && (!defined(__STDC_VERSION__) || (__STDC_VERSION__ < 201112L))
#  if defined(_MSC_VER)
#    include <assert.h>
#    if defined(static_assert)
#      define _Static_assert(cond, msg) static_assert((cond), msg)
#    else
#      define _Static_assert(cond, msg) \
        typedef char Q_STATIC_ASSERT_CONCAT(q_static_assert_line_, __LINE__)[(cond) ? 1 : -1]
#    endif
#  else
#    define _Static_assert(cond, msg) \
      typedef char Q_STATIC_ASSERT_CONCAT(q_static_assert_line_, __LINE__)[(cond) ? 1 : -1]
#  endif
#endif

#ifndef Q3_VM
#include <stdbool.h>
#include <stddef.h>
#endif

#ifndef STATIC_ASSERT
#  if defined(__cplusplus)
#    define STATIC_ASSERT(cond, msg) static_assert((cond), msg)
#  else
#    define STATIC_ASSERT(cond, msg) _Static_assert((cond), msg)
#  endif
#endif

#ifndef ARRAY_LEN
#  define ARRAY_LEN(x) (sizeof(x) / sizeof((x)[0]))
#endif

#if defined(__has_attribute)
#  if __has_attribute(fallthrough)
#    define FALLTHROUGH __attribute__((fallthrough))
#  else
#    define FALLTHROUGH ((void)0)
#  endif
#else
#  define FALLTHROUGH ((void)0)
#endif

#if defined(__GNUC__) || defined(__clang__)
#  define LIKELY(x) (__builtin_expect(!!(x), 1))
#  define UNLIKELY(x) (__builtin_expect(!!(x), 0))
#else
#  define LIKELY(x) (x)
#  define UNLIKELY(x) (x)
#endif

#define Q_MIN(a, b) ((a) < (b) ? (a) : (b))
#define Q_MAX(a, b) ((a) > (b) ? (a) : (b))

#endif // Q_COMPAT_H
