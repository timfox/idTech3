#!/usr/bin/env bash
# Regression checks for pk3-backed native module loading and script fallback paths.
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
lua_debug_c = root / "src/qcommon/lua_debug.c"


def fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)
    sys.exit(1)


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//.*", "", text)


def find_function_body(text: str, name: str) -> str:
    text_no_comments = strip_comments(text)
    match = re.search(r"(?m)^[A-Za-z_][A-Za-z0-9_\s\*]*\b" + re.escape(name) + r"\s*\([^;]*\)\s*\{", text_no_comments)
    if not match:
        fail(f"{name}: function definition not found")

    start = text_no_comments.find("{", match.start())
    if start < 0:
        fail(f"{name}: opening brace not found")

    depth = 0
    i = start
    state = "code"
    while i < len(text_no_comments):
        ch = text_no_comments[i]
        if state == "code":
            if ch == '"':
                state = "string"
            elif ch == "'":
                state = "char"
            elif ch == "{":
                depth += 1
            elif ch == "}":
                depth -= 1
                if depth == 0:
                    return text_no_comments[start : i + 1]
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
    first_pos = haystack.find(first)
    second_pos = haystack.find(second)
    if first_pos < 0 or second_pos < 0 or first_pos >= second_pos:
        fail(f"{context}: expected {first!r} before {second!r}")


def assert_count(haystack: str, needle: str, expected: int, context: str) -> None:
    actual = haystack.count(needle)
    if actual != expected:
        fail(f"{context}: expected {expected} occurrences of {needle!r}, found {actual}")


for path in (files_c, lua_debug_c):
    if not path.is_file():
        fail(f"missing source file: {path}")

files_text = files_c.read_text()
files_uncommented = strip_comments(files_text)

assert_contains(
    files_uncommented,
    '#define FS_NATIVE_LIB_CACHE_PREFIX "vm/native_cache/"',
    "native cache prefix",
)

startup_body = find_function_body(files_text, "FS_Startup")
assert_regex(
    startup_body,
    r'com_nativeLibraryExtractPk3\s*=\s*Cvar_Get\s*\(\s*"com_nativeLibraryExtractPk3"\s*,\s*"1"\s*,\s*CVAR_ARCHIVE\s*\)',
    "com_nativeLibraryExtractPk3 default",
)
assert_contains(
    startup_body,
    "Cvar_SetDescription( com_nativeLibraryExtractPk3,",
    "com_nativeLibraryExtractPk3 description",
)
assert_contains(
    startup_body,
    'Com_Printf( "com_nativeLibraryExtractPk3: extracting embedded native libs from pk3 is enabled.\\n" );',
    "com_nativeLibraryExtractPk3 startup log",
)

pk3_body = find_function_body(files_text, "FS_TryLoadLibraryFromPk3Cache")
assert_contains(
    pk3_body,
    "if ( !com_nativeLibraryExtractPk3 || !com_nativeLibraryExtractPk3->integer )",
    "pk3 extraction cvar gate",
)
assert_contains(pk3_body, "if ( !name || !name[0] )", "pk3 extraction empty-name guard")
assert_contains(pk3_body, "slash = strrchr( name, '/' );", "pk3 cache basename slash detection")
assert_contains(pk3_body, "base = slash + 1;", "pk3 cache basename isolation")
assert_contains(pk3_body, 'strstr( base, ".dll" )', "Windows native extension filter")
assert_contains(pk3_body, 'Q_stricmp( base + strlen( base ) - 3, ".so" )', "POSIX native extension filter")
assert_contains(
    pk3_body,
    'Com_sprintf( cacheQpath, sizeof( cacheQpath ), "%s%s", FS_NATIVE_LIB_CACHE_PREFIX, base );',
    "basename-only native cache qpath",
)
assert_contains(pk3_body, "len = FS_ReadFile( name, &fileBuf );", "direct pk3 library read")
assert_contains(pk3_body, "if ( slash )", "explicit path must not try implicit pk3 fallbacks")
assert_contains(pk3_body, 'Com_sprintf( alt, sizeof( alt ), "vm/%s", name );', "vm/ pk3 fallback path")
assert_contains(pk3_body, 'Com_sprintf( alt, sizeof( alt ), "modules/%s", name );', "modules/ pk3 fallback path")
assert_order(pk3_body, "len = FS_ReadFile( name, &fileBuf );", 'Com_sprintf( alt, sizeof( alt ), "vm/%s", name );', "direct read before vm/ fallback")
assert_order(pk3_body, 'Com_sprintf( alt, sizeof( alt ), "vm/%s", name );', 'Com_sprintf( alt, sizeof( alt ), "modules/%s", name );', "vm/ fallback before modules/ fallback")
assert_count(pk3_body, "FS_ReadFile(", 3, "pk3 read attempts")

assert_contains(pk3_body, "crcPak = crc32_buffer( (const byte *)fileBuf, (unsigned int)len );", "pk3 CRC calculation")
assert_contains(
    pk3_body,
    "Q_strncpyz( osCachePath, FS_BuildOSPath( fs_homepath->string, fs_gamedir, cacheQpath ), sizeof( osCachePath ) );",
    "native cache writes under fs_homepath/fs_gamedir",
)
assert_contains(pk3_body, 'fp = Sys_FOpen( osCachePath, "rb" );', "native cache reuse open")
assert_contains(pk3_body, "readLen > 0 && readLen == len", "native cache length gate")
assert_contains(pk3_body, "crcDisk = crc32_buffer( (const byte *)diskBuf, (unsigned int)readLen );", "native cache CRC calculation")
assert_contains(pk3_body, "if ( crcDisk == crcPak )", "native cache CRC reuse gate")
assert_order(pk3_body, "if ( crcDisk == crcPak )", "h = FS_TryLoadLibraryPath( osCachePath );", "dlopen only after cache CRC validation")
assert_contains(pk3_body, "FS_CreatePath( osCachePath )", "native cache create path")
assert_contains(pk3_body, 'FILE *out = Sys_FOpen( osCachePath, "wb" );', "direct native cache write open")
assert_contains(pk3_body, "fwrite( fileBuf, 1, (size_t)len, out )", "direct native cache write")
assert_contains(pk3_body, "FS_FOpenFileWrite rejects .so paths", "direct write rationale")
assert_contains(pk3_body, "FS_LoadLibrary: using pk3 native cache", "cache reuse diagnostic")
assert_contains(pk3_body, "FS_LoadLibrary: failed to write pk3 native cache", "cache write failure diagnostic")
assert_contains(pk3_body, "FS_LoadLibrary: extracted pk3 native lib to", "cache extraction diagnostic")

load_body = find_function_body(files_text, "FS_LoadLibrary")
assert_contains(load_body, "libHandle = FS_TryLoadLibraryFromPk3Cache( name );", "FS_LoadLibrary pk3 cache probe")
assert_order(load_body, "libHandle = FS_TryLoadLibraryFromPk3Cache( name );", "while ( !libHandle && sp )", "pk3 cache before loose filesystem search")

lua_text = lua_debug_c.read_text()
lua_body = find_function_body(lua_text, "LuaDebug_LoadScript")
assert_contains(lua_body, "if ( luaL_loadfile( s_luaState, scriptPath ) != LUA_OK )", "Lua file load primary path")
assert_contains(lua_body, "lua_pop( s_luaState, 1 );", "Lua clears failed loadfile error before pk3 fallback")
assert_contains(lua_body, "blen = FS_ReadFile( scriptPath, &buf );", "Lua pk3 script read fallback")
assert_contains(lua_body, "if ( luaL_loadbuffer( s_luaState, (const char *)buf, (size_t)blen, scriptPath ) != LUA_OK )", "Lua pk3 script loadbuffer fallback")
assert_contains(lua_body, "FS_FreeFile( buf );", "Lua frees pk3 fallback buffer")
assert_order(lua_body, "luaL_loadfile", "FS_ReadFile", "Lua loadfile failure before filesystem fallback")
assert_order(lua_body, "FS_ReadFile", "luaL_loadbuffer", "Lua filesystem fallback before loadbuffer")

print("PASS: test_native_module_regressions")
PY
