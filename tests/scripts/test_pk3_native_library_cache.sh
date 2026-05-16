#!/usr/bin/env bash
# Regression checks for pk3-backed native module extraction/cache invariants.
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


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//.*", "", text)


def find_function_body(text: str, name: str) -> str:
    pattern = re.compile(
        r"^[A-Za-z_][A-Za-z0-9_\s\*]*\b" + re.escape(name) + r"\s*\(",
        re.M,
    )
    match = pattern.search(text)
    if not match:
        fail(f"{name}: function definition not found")

    start = text.find("{", match.end())
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


def assert_contains(haystack: str, needle: str, context: str) -> None:
    if needle not in haystack:
        fail(f"{context}: expected {needle!r}")


def assert_regex(haystack: str, pattern: str, context: str) -> None:
    if not re.search(pattern, haystack, flags=re.S):
        fail(f"{context}: expected pattern {pattern!r}")


def assert_order(haystack: str, first: str, second: str, context: str) -> None:
    first_index = haystack.find(first)
    second_index = haystack.find(second)
    if first_index < 0:
        fail(f"{context}: expected {first!r}")
    if second_index < 0:
        fail(f"{context}: expected {second!r}")
    if first_index >= second_index:
        fail(f"{context}: expected {first!r} before {second!r}")


if not files_c.is_file():
    fail(f"missing source file: {files_c}")

text = files_c.read_text()
uncommented = strip_comments(text)
startup = find_function_body(text, "FS_Startup")
cache = find_function_body(text, "FS_TryLoadLibraryFromPk3Cache")
load_library = find_function_body(text, "FS_LoadLibrary")
cache_uncommented = strip_comments(cache)

assert_contains(
    startup,
    'com_nativeLibraryExtractPk3 = Cvar_Get( "com_nativeLibraryExtractPk3", "1", CVAR_ARCHIVE );',
    "FS_Startup",
)
assert_contains(
    startup,
    "Cvar_SetDescription( com_nativeLibraryExtractPk3,",
    "FS_Startup",
)
assert_contains(
    startup,
    'Com_Printf( "com_nativeLibraryExtractPk3: extracting embedded native libs from pk3 is enabled.\\n" );',
    "FS_Startup",
)

assert_contains(
    text,
    '#define FS_NATIVE_LIB_CACHE_PREFIX "vm/native_cache/"',
    "native cache prefix",
)
assert_regex(
    cache,
    r"if\s*\(\s*!com_nativeLibraryExtractPk3\s*\|\|\s*!com_nativeLibraryExtractPk3->integer\s*\)\s*\{\s*return NULL;",
    "FS_TryLoadLibraryFromPk3Cache cvar gate",
)
assert_regex(
    cache,
    r"slash\s*=\s*strrchr\s*\(\s*name\s*,\s*'/'\s*\).*?if\s*\(\s*slash\s*\)\s*\{\s*base\s*=\s*slash\s*\+\s*1;",
    "FS_TryLoadLibraryFromPk3Cache basename extraction",
)
assert_contains(
    cache,
    'Com_sprintf( cacheQpath, sizeof( cacheQpath ), "%s%s", FS_NATIVE_LIB_CACHE_PREFIX, base );',
    "FS_TryLoadLibraryFromPk3Cache basename-only cache qpath",
)
assert_contains(
    cache,
    "Q_strncpyz( osCachePath, FS_BuildOSPath( fs_homepath->string, fs_gamedir, cacheQpath ), sizeof( osCachePath ) );",
    "FS_TryLoadLibraryFromPk3Cache homepath cache location",
)
assert_regex(
    cache,
    r"#if defined\(\s*_WIN32\s*\).*?strstr\s*\(\s*base\s*,\s*\"\.dll\"\s*\).*?#else.*?Q_stricmp\s*\(\s*base\s*\+\s*strlen\s*\(\s*base\s*\)\s*-\s*3\s*,\s*\"\.so\"\s*\)",
    "FS_TryLoadLibraryFromPk3Cache native extension checks",
)

read_calls = re.findall(r"\bFS_ReadFile\s*\(", cache_uncommented)
if len(read_calls) != 3:
    fail(f"FS_TryLoadLibraryFromPk3Cache: expected exactly three FS_ReadFile attempts, found {len(read_calls)}")
assert_order(
    cache,
    "len = FS_ReadFile( name, &fileBuf );",
    'Com_sprintf( alt, sizeof( alt ), "vm/%s", name );',
    "FS_TryLoadLibraryFromPk3Cache direct before vm fallback",
)
assert_order(
    cache,
    'Com_sprintf( alt, sizeof( alt ), "vm/%s", name );',
    'Com_sprintf( alt, sizeof( alt ), "modules/%s", name );',
    "FS_TryLoadLibraryFromPk3Cache vm before modules fallback",
)
assert_regex(
    cache,
    r"if\s*\(\s*slash\s*\)\s*\{\s*return NULL;\s*\}",
    "FS_TryLoadLibraryFromPk3Cache no fallback for explicit paths",
)

assert_order(
    cache,
    "crcPak = crc32_buffer( (const byte *)fileBuf, (unsigned int)len );",
    "crcDisk = crc32_buffer( (const byte *)diskBuf, (unsigned int)readLen );",
    "FS_TryLoadLibraryFromPk3Cache CRC comparison setup",
)
assert_regex(
    cache,
    r"readLen\s*>\s*0\s*&&\s*readLen\s*==\s*len",
    "FS_TryLoadLibraryFromPk3Cache disk length gate",
)
assert_order(
    cache,
    "if ( crcDisk == crcPak )",
    "h = FS_TryLoadLibraryPath( osCachePath );",
    "FS_TryLoadLibraryFromPk3Cache load only after CRC match",
)
assert_contains(
    cache,
    "/* FS_FOpenFileWrite rejects .so paths; write cache file directly (see FS_CheckFilenameIsNotAllowed). */",
    "FS_TryLoadLibraryFromPk3Cache direct write rationale",
)
assert_order(
    cache,
    "if ( FS_CreatePath( osCachePath ) )",
    'Sys_FOpen( osCachePath, "wb" )',
    "FS_TryLoadLibraryFromPk3Cache creates cache path before write",
)
assert_contains(
    cache,
    'Com_Printf( S_COLOR_YELLOW "FS_LoadLibrary: failed to write pk3 native cache %s\\n", osCachePath );',
    "FS_TryLoadLibraryFromPk3Cache warning log",
)
assert_contains(
    cache,
    'Com_Printf( "FS_LoadLibrary: extracted pk3 native lib to %s\\n", cacheQpath );',
    "FS_TryLoadLibraryFromPk3Cache extraction log",
)
assert_contains(
    cache,
    'Com_Printf( "FS_LoadLibrary: using pk3 native cache %s\\n", cacheQpath );',
    "FS_TryLoadLibraryFromPk3Cache cache reuse log",
)

assert_order(
    load_library,
    "libHandle = FS_TryLoadLibraryFromPk3Cache( name );",
    "while ( !libHandle && sp )",
    "FS_LoadLibrary pk3 cache before loose filesystem search",
)

if "FS_TryLoadLibraryFromPk3Cache" not in uncommented:
    fail("FS_TryLoadLibraryFromPk3Cache unexpectedly appears only in comments")

print("PASS: test_pk3_native_library_cache")
PY
