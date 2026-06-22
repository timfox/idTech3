/* Minimal config.h for vendored libopusfile */
#ifndef OPUSFILE_CONFIG_H
#define OPUSFILE_CONFIG_H

#define HAVE_STDLIB_H 1
#define HAVE_STDINT_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_MEMORY_H 1
#define STDC_HEADERS 1

/* Disable HTTP support to avoid extra dependencies. */
#define OP_DISABLE_HTTP 1

#endif /* OPUSFILE_CONFIG_H */
