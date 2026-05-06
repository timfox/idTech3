#!/usr/bin/env bash
# Regression checks for pk3-backed native module extraction/cache behavior.
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


def find_function_body(text: str, name: str) -> str:
    match = re.search(r"\b" + re.escape(name) + r"\s*\(", text)
    if not match:
        fail(f"{name}: function not found")

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


def assert_ordered(haystack: str, needles: list[str], context: str) -> None:
    pos = -1
    for needle in needles:
        next_pos = haystack.find(needle, pos + 1)
        if next_pos < 0:
            fail(f"{context}: expected {needle!r} after offset {pos}")
        pos = next_pos


if not files_c.is_file():
    fail(f"missing source file: {files_c}")

text = files_c.read_text()
startup = find_function_body(text, "FS_Startup")
cache = find_function_body(text, "FS_TryLoadLibraryFromPk3Cache")
load_library = find_function_body(text, "FS_LoadLibrary")

assert_contains(text, 'static\tcvar_t\t\t*com_nativeLibraryExtractPk3;', "files.c")
assert_contains(text, '#define FS_NATIVE_LIB_CACHE_PREFIX "vm/native_cache/"', "files.c")

assert_contains(
    startup,
    'com_nativeLibraryExtractPk3 = Cvar_Get( "com_nativeLibraryExtractPk3", "1", CVAR_ARCHIVE );',
    "FS_Startup",
)
assert_contains(startup, "Cvar_SetDescription( com_nativeLibraryExtractPk3,", "FS_Startup")
assert_contains(
    startup,
    'Com_Printf( "com_nativeLibraryExtractPk3: extracting embedded native libs from pk3 is enabled.\\n" );',
    "FS_Startup",
)

assert_contains(
    cache,
    "if ( !com_nativeLibraryExtractPk3 || !com_nativeLibraryExtractPk3->integer )",
    "FS_TryLoadLibraryFromPk3Cache",
)
assert_contains(cache, "if ( !name || !name[0] )", "FS_TryLoadLibraryFromPk3Cache")
assert_contains(cache, 'Q_stricmp( base + strlen( base ) - 3, ".so" ) != 0', "FS_TryLoadLibraryFromPk3Cache")
assert_contains(cache, "strstr( base, \".dll\" )", "FS_TryLoadLibraryFromPk3Cache")
assert_contains(
    cache,
    'Com_sprintf( cacheQpath, sizeof( cacheQpath ), "%s%s", FS_NATIVE_LIB_CACHE_PREFIX, base );',
    "FS_TryLoadLibraryFromPk3Cache",
)
assert_contains(
    cache,
    "Q_strncpyz( osCachePath, FS_BuildOSPath( fs_homepath->string, fs_gamedir, cacheQpath ), sizeof( osCachePath ) );",
    "FS_TryLoadLibraryFromPk3Cache",
)

read_calls = re.findall(r"\bFS_ReadFile\s*\(", cache)
if len(read_calls) != 3:
    fail(f"FS_TryLoadLibraryFromPk3Cache: expected exactly 3 FS_ReadFile attempts, found {len(read_calls)}")
assert_ordered(
    cache,
    [
        "len = FS_ReadFile( name, &fileBuf );",
        'Com_sprintf( alt, sizeof( alt ), "vm/%s", name );',
        "len = FS_ReadFile( alt, &fileBuf );",
        'Com_sprintf( alt, sizeof( alt ), "modules/%s", name );',
        "len = FS_ReadFile( alt, &fileBuf );",
    ],
    "FS_TryLoadLibraryFromPk3Cache",
)
assert_regex(cache, r"if\s*\(\s*slash\s*\)\s*\{\s*return NULL;", "FS_TryLoadLibraryFromPk3Cache")

assert_ordered(
    cache,
    [
        "crcPak = crc32_buffer( (const byte *)fileBuf, (unsigned int)len );",
        'fp = Sys_FOpen( osCachePath, "rb" );',
        "readLen = ftell( fp );",
        "readLen > 0 && readLen == len",
        "crcDisk = crc32_buffer( (const byte *)diskBuf, (unsigned int)readLen );",
        "if ( crcDisk == crcPak )",
        "FS_FreeFile( fileBuf );",
        "h = FS_TryLoadLibraryPath( osCachePath );",
        'Com_Printf( "FS_LoadLibrary: using pk3 native cache %s\\n", cacheQpath );',
    ],
    "FS_TryLoadLibraryFromPk3Cache cache-reuse path",
)

assert_ordered(
    cache,
    [
        "/* FS_FOpenFileWrite rejects .so paths; write cache file directly",
        "if ( FS_CreatePath( osCachePath ) )",
        'FILE *out = Sys_FOpen( osCachePath, "wb" );',
        "fwrite( fileBuf, 1, (size_t)len, out )",
        'Com_Printf( S_COLOR_YELLOW "FS_LoadLibrary: failed to write pk3 native cache %s\\n", osCachePath );',
        "h = FS_TryLoadLibraryPath( osCachePath );",
        'Com_Printf( "FS_LoadLibrary: extracted pk3 native lib to %s\\n", cacheQpath );',
    ],
    "FS_TryLoadLibraryFromPk3Cache extraction path",
)

assert_ordered(
    load_library,
    [
        "libHandle = FS_TryLoadLibraryFromPk3Cache( name );",
        "if ( libHandle )",
        "return libHandle;",
        "while ( !libHandle && sp )",
    ],
    "FS_LoadLibrary",
)

print("PASS: test_pk3_native_library_cache")
PY
