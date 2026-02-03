# Security Best Practices Review

## Executive Summary
This codebase is primarily C/C++ (Quake 3–derived engine plus renderer/server/client modules). The security-best-practices skill references do not include C/C++ guidance, so this review is a best‑effort pass focused on common C/C++ memory-safety and input‑handling risks.

Key findings:
- High‑risk unbounded formatting is present (`vsprintf`) in core shared utilities.
- Multiple unbounded `sprintf` uses remain in server logic; these rely on implicit size constraints rather than enforced bounds.
- Several string concatenation sites use ad‑hoc length checks; these are easy to regress and should be replaced with size‑aware helpers for secure‑by‑default behavior.

## High Severity

**HIGH-1: Unbounded `vsprintf` in core formatting helpers**
- **Why this matters:** `vsprintf` performs no bounds checking. If formatted output exceeds the destination buffer, it can overflow and corrupt memory before any length checks occur.
- **Impact:** Memory corruption leading to crashes; if an attacker can influence formatted content, this can become a code‑execution vector.
- **Evidence:**
  - `src/qcommon/q_shared.c:1730` (`vsprintf( bigbuffer, fmt, argptr );`)
  - `src/qcommon/q_shared.c:1780` (`vsprintf( buf, format, argptr );`)
- **Secure‑by‑default improvement:** Replace these with `vsnprintf` (or a project wrapper that enforces buffer sizes), and propagate buffer sizes through call sites. Consider returning an error or truncating safely.

## Medium Severity

**MED-1: Unbounded `sprintf` while serializing filter rules**
- **Why this matters:** `sprintf` into a fixed‑size stack buffer (`buf`) relies on implicit size assumptions about `node->p1` and `node->p2`. If those assumptions change, buffer overflow is possible.
- **Evidence:**
  - `src/server/sv_filter.c:224`
  - `src/server/sv_filter.c:237`
  - `src/server/sv_filter.c:247`
  - `src/server/sv_filter.c:249`
  - `src/server/sv_filter.c:253`
- **Secure‑by‑default improvement:** Replace with `Com_sprintf`/`Q_snprintf`-style wrappers and enforce max lengths for `node->p1`/`node->p2` at parse time.

**MED-2: Unbounded `sprintf` into client command buffers**
- **Why this matters:** `sprintf` writes into a client command buffer based on TLD/country strings. These strings are assumed to be short, but the buffer size is not enforced at the write site.
- **Evidence:**
  - `src/server/sv_client.c:400`
  - `src/server/sv_client.c:402`
- **Secure‑by‑default improvement:** Use bounded formatting (`Com_sprintf`/`snprintf`) with the known buffer size, or explicitly clamp source strings.

## Low Severity

**LOW-1: Ad‑hoc length checks with unbounded string ops in console commands**
- **Why this matters:** Manual `len > 1000` checks are decoupled from the actual buffer size and still rely on `strcpy`/`strcat`. This is safe today, but easy to break as constants evolve.
- **Evidence:**
  - `src/server/sv_ccmds.c:1338`
  - `src/server/sv_ccmds.c:1339`
  - `src/server/sv_ccmds.c:1384`
  - `src/server/sv_ccmds.c:1385`
- **Secure‑by‑default improvement:** Replace with `Q_strncpyz`/`Q_strcat` or `Com_sprintf` using `MAX_STRING_CHARS` to make the bound explicit and future‑proof.

## Secure‑By‑Default Improvements (Defense‑in‑Depth)
- Introduce a single safe string API (e.g., `Q_snprintf`, `Q_strncpyz`, `Q_strcat`) and discourage raw `strcpy`/`strcat`/`sprintf` via macros or compiler warnings.
- Add compiler hardening flags in CMake for release builds (stack protector, FORTIFY, RELRO, PIE) and enable warnings like `-Wformat-security`, `-Wstringop-overflow`, `-Wstringop-truncation` in CI.
- Add fuzzing harnesses for parsers (BSP, VM bytecode, network packet parsing) and run them in CI or integrate with OSS-Fuzz style workflows.
- Run static analyzers (clang‑tidy, cppcheck) with security‑focused rulesets to prevent regressions.

