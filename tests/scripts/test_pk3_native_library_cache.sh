#!/usr/bin/env bash
# Regression checks for pk3-backed native module extraction/cache loading.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

command -v python3 >/dev/null 2>&1 || fail "python3 not in PATH"

python3 - "$PROJECT_ROOT" <<'PY'
import re
import sys
from pathlib import Path

root = Path(sys.argv[1])
files_c = root / "src/qcommon/files.c"


def fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)
    sys.exit(1)


def assert_contains(haystack: str, needle: str, context: str) -> None:
    if needle not in haystack:
        fail(f"{context}: expected {needle!r}")


def assert_not_contains(haystack: str, needle: str, context: str) -> None:
    if needle in haystack:
        fail(f"{context}: unexpected {needle!r}")


def assert_regex(haystack: str, pattern: str, context: str) -> None:
    if not re.search(pattern, haystack, flags=re.S):
        fail(f"{context}: expected pattern {pattern!r}")


def assert_count(haystack: str, needle: str, expected: int, context: str) -> None:
    actual = haystack.count(needle)
    if actual != expected:
        fail(f"{context}: expected {expected} occurrences of {needle!r}, found {actual}")


def assert_order(haystack: str, first: str, second: str, context: str) -> None:
    first_idx = haystack.find(first)
    second_idx = haystack.find(second)
    if first_idx < 0 or second_idx < 0 or first_idx >= second_idx:
        fail(f"{context}: expected {first!r} before {second!r}")


def find_function_body(text: str, name: str) -> str:
    pattern = re.compile(
        r"^[^\n;{]*\b" + re.escape(name) + r"\s*\([^;\n]*\)\s*\{",
        flags=re.M,
    )
    match = pattern.search(text)
    if not match:
        fail(f"{name}: function definition not found")

    start = text.find("{", match.start())
    if start < 0:
        fail(f"{name}: opening brace not found")

    depth = 0
    i = start
    state = "code"
    while i < len(text):
        ch = text[i]
        nxt = text[i + 1] if i + 1 < len(text) else ""

        if state == "code":
            if ch == "/" and nxt == "/":
                state = "line_comment"
                i += 2
                continue
            if ch == "/" and nxt == "*":
                state = "block_comment"
                i += 2
                continue
            if ch == '"':
                state = "string"
                i += 1
                continue
            if ch == "'":
                state = "char"
                i += 1
                continue
            if ch == "{":
                depth += 1
            elif ch == "}":
                depth -= 1
                if depth == 0:
                    return text[start : i + 1]
        elif state == "line_comment":
            if ch == "\n":
                state = "code"
        elif state == "block_comment":
            if ch == "*" and nxt == "/":
                state = "code"
                i += 2
                continue
        elif state == "string":
            if ch == "\\":
                i += 2
                continue
            if ch == '"':
                state = "code"
        elif state == "char":
            if ch == "\\":
                i += 2
                continue
            if ch == "'":
                state = "code"
        i += 1

    fail(f"{name}: closing brace not found")


if not files_c.is_file():
    fail(f"missing source file: {files_c}")

text = files_c.read_text()
startup = find_function_body(text, "FS_Startup")
cache_body = find_function_body(text, "FS_TryLoadLibraryFromPk3Cache")
load_body = find_function_body(text, "FS_LoadLibrary")

assert_contains(text, "static\tcvar_t\t\t*com_nativeLibraryExtractPk3;", "pk3 native extraction cvar declaration")
assert_contains(text, '#define FS_NATIVE_LIB_CACHE_PREFIX "vm/native_cache/"', "pk3 native cache prefix")

assert_contains(
    startup,
    'com_nativeLibraryExtractPk3 = Cvar_Get( "com_nativeLibraryExtractPk3", "1", CVAR_ARCHIVE );',
    "pk3 native extraction cvar default",
)
assert_contains(
    startup,
    "native .so/.dll modules referenced only from .pk3 are extracted to vm/native_cache/",
    "pk3 native extraction cvar description",
)
assert_contains(
    startup,
    'Com_Printf( "com_nativeLibraryExtractPk3: extracting embedded native libs from pk3 is enabled.\\n" );',
    "pk3 native extraction startup log",
)

assert_contains(
    cache_body,
    "if ( !com_nativeLibraryExtractPk3 || !com_nativeLibraryExtractPk3->integer ) {",
    "pk3 native extraction kill switch",
)
assert_contains(cache_body, "if ( !name || !name[0] ) {", "pk3 native name guard")
assert_contains(cache_body, "base = name;", "pk3 native basename initialization")
assert_contains(cache_body, "slash = strrchr( name, '/' );", "pk3 native slash split")
assert_contains(cache_body, "base = slash + 1;", "pk3 native basename extraction")
assert_contains(
    cache_body,
    'Com_sprintf( cacheQpath, sizeof( cacheQpath ), "%s%s", FS_NATIVE_LIB_CACHE_PREFIX, base );',
    "pk3 native basename-only cache path",
)
assert_contains(cache_body, 'strstr( base, ".dll" )', "pk3 native Windows extension filter")
assert_contains(
    cache_body,
    'Q_stricmp( base + strlen( base ) - 3, ".so" ) != 0',
    "pk3 native Unix extension filter",
)

assert_count(cache_body, "len = FS_ReadFile(", 3, "pk3 native direct/vm/modules probes")
assert_order(
    cache_body,
    "len = FS_ReadFile( name, &fileBuf );",
    'Com_sprintf( alt, sizeof( alt ), "vm/%s", name );',
    "pk3 native direct probe before vm fallback",
)
assert_order(
    cache_body,
    'Com_sprintf( alt, sizeof( alt ), "vm/%s", name );',
    'Com_sprintf( alt, sizeof( alt ), "modules/%s", name );',
    "pk3 native vm fallback before modules fallback",
)
assert_regex(
    cache_body,
    r"if\s*\(\s*slash\s*\)\s*\{\s*return NULL;\s*\}\s*Com_sprintf\(\s*alt,\s*sizeof\(\s*alt\s*\),\s*\"vm/%s\"",
    "pk3 native explicit paths do not fall back to vm/modules",
)

assert_contains(
    cache_body,
    "crcPak = crc32_buffer( (const byte *)fileBuf, (unsigned int)len );",
    "pk3 native pk3 CRC calculation",
)
assert_contains(cache_body, "readLen = ftell( fp );", "pk3 native cache length check")
assert_contains(
    cache_body,
    "if ( readLen > 0 && readLen == len ) {",
    "pk3 native cache length must match pk3 payload",
)
assert_contains(
    cache_body,
    "crcDisk = crc32_buffer( (const byte *)diskBuf, (unsigned int)readLen );",
    "pk3 native disk CRC calculation",
)
assert_order(
    cache_body,
    "if ( crcDisk == crcPak ) {",
    "h = FS_TryLoadLibraryPath( osCachePath );",
    "pk3 native cache reuse requires CRC match before loading",
)
assert_contains(cache_body, 'Sys_FOpen( osCachePath, "wb" )', "pk3 native direct cache write")
assert_contains(cache_body, "fwrite( fileBuf, 1, (size_t)len, out )", "pk3 native payload write")
assert_contains(
    cache_body,
    "FS_FOpenFileWrite rejects .so paths; write cache file directly",
    "pk3 native direct write rationale",
)
assert_not_contains(cache_body, "FS_WriteFile", "pk3 native cache writer avoids FS_WriteFile .so rejection")
assert_contains(cache_body, "FS_LoadLibrary: using pk3 native cache", "pk3 native cache reuse log")
assert_contains(cache_body, "FS_LoadLibrary: extracted pk3 native lib", "pk3 native extraction log")

assert_order(
    load_body,
    "libHandle = FS_TryLoadLibraryFromPk3Cache( name );",
    "while ( !libHandle && sp ) {",
    "FS_LoadLibrary tries pk3 cache before loose filesystem paths",
)

print("PASS: test_pk3_native_library_cache")
PY
