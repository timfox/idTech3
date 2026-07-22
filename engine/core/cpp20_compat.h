#pragma once

/*
 * C++20 migration compatibility helpers (C and C++ includable).
 * See docs/CPP20_ABI_BOUNDARIES.md and docs/CPP20_MIGRATION.md.
 *
 * Use IDTECH3_EXTERN_C_BEGIN/END around real C ABI declarations only.
 * Do not wrap C++ classes, templates, namespaces, or overloaded functions.
 */

#ifdef __cplusplus
#define IDTECH3_EXTERN_C_BEGIN extern "C" {
#define IDTECH3_EXTERN_C_END }
#else
#define IDTECH3_EXTERN_C_BEGIN
#define IDTECH3_EXTERN_C_END
#endif

/* restrict is C-only; empty in C++. */
#ifdef __cplusplus
#ifndef Q_RESTRICT
#define Q_RESTRICT
#endif
#else
#ifndef Q_RESTRICT
#define Q_RESTRICT restrict
#endif
#endif

/* Prefer nullptr in C++ TUs; NULL remains valid at C boundaries. */
#ifdef __cplusplus
#ifndef Q_NULLPTR
#define Q_NULLPTR nullptr
#endif
#else
#ifndef Q_NULLPTR
#define Q_NULLPTR NULL
#endif
#endif
