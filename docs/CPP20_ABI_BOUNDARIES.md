# C++20 ABI Boundaries

**Status:** Foundation  
**Audience:** Anyone converting a TU or touching shared headers

## Principle

External modules, QVMs, native game DLLs, renderer plugins, tools, and third-party C libraries continue to see **unmangled C symbols** and **unchanged structure layouts**.

Internal C++20 code may use namespaces, `bool`, RAII, etc., behind those façades.

## Compatibility header

[`engine/core/cpp20_compat.h`](../engine/core/cpp20_compat.h):

```c
#ifdef __cplusplus
extern "C" {
#endif
/* C declarations */
#ifdef __cplusplus
}
#endif
```

Also provides `Q_RESTRICT` (empty in C++, `restrict` in C).

Apply `extern "C"` only to real C ABI boundaries — never to C++ classes, templates, or overloaded functions.

## KEEP_C_EXTERNAL_BOUNDARY (do not C++-ify types)

| Surface | Notes |
|---------|--------|
| `playerState_t`, `entityState_t`, `usercmd_t` | Network / demo / QVM |
| `trace_t` | Collision ABI |
| `netadr_t`, `msg_t` | Networking |
| `cvar_t` | Engine/mod cvar system |
| `vm_t` / VM traps | QVM bridge |
| `refEntity_t`, `refdef_t`, `refimport_t` / `refexport_t` | Renderer plugin ABI |
| Native game module exports (`GetGameAPI`, …) | Dynamic load |
| Platform callbacks (SDL, OS) | C calling convention |

## ABI guards

[`engine/core/cpp20_abi_guards.cpp`](../engine/core/cpp20_abi_guards.cpp) compile-time checks (Linux x86_64 baseline):

| Type | Expected `sizeof` (linux amd64) |
|------|--------------------------------:|
| `qboolean` | 1 |
| `trace_t` | 76 |
| `usercmd_t` | 24 |
| `entityState_t` | 208 |
| `playerState_t` | 468 |
| `netadr_t` | 28 |
| `msg_t` | 40 |

Other platforms must extend the `#if` tables rather than weakening asserts.

**Rules:**

- Do not substitute `bool` into ABI structs that historically used `qboolean`/`int` without an explicit versioned ABI bump.
- Do not introduce `size_t`/`ptrdiff_t` into fixed network or snapshot records without review.
- Do not change packing, enum underlying widths, or field order.

## Symbol policy

Converted `.cpp` leaves that export C functions must define them with C linkage (`extern "C"` block or per-function).

Verify with:

```bash
./tests/scripts/test_cpp20_symbols.sh
nm -C release/idtech3 | grep Com_BlockChecksum   # should show unmangled
```

## Dual inclusion

Public headers must compile from both a minimal `.c` and a minimal `.cpp` TU:

```bash
./tests/scripts/test_cpp20_headers.sh
```

Intentionally C-only headers must be listed in that script’s deny list with a comment.
