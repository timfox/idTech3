#!/usr/bin/env bash
# Regression checks for native VM module loading and pk3-backed native library caching.
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
vm_path = root / "src/qcommon/vm.c"
files_path = root / "src/qcommon/files.c"


def fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)
    sys.exit(1)


def read(path: Path) -> str:
    if not path.is_file():
        fail(f"missing source file: {path}")
    return path.read_text()


def find_function_body(text: str, name: str) -> str:
    match = re.search(
        r"(?m)^[\t ]*(?:static[\t ]+)?[A-Za-z_][A-Za-z0-9_\t \*]*\b"
        + re.escape(name)
        + r"\s*\([^()\n;{}]*\)\s*\{",
        text,
    )
    if not match:
        fail(f"{name}: function definition not found")

    start = match.end() - 1

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


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//.*", "", text)


def assert_contains(haystack: str, needle: str, context: str) -> None:
    if needle not in haystack:
        fail(f"{context}: expected {needle!r}")


def assert_not_contains(haystack: str, needle: str, context: str) -> None:
    if needle in haystack:
        fail(f"{context}: unexpected {needle!r}")


def assert_regex(haystack: str, pattern: str, context: str) -> None:
    if not re.search(pattern, haystack, flags=re.S):
        fail(f"{context}: expected pattern {pattern!r}")


def assert_order(haystack: str, needles: list[str], context: str) -> None:
    position = -1
    for needle in needles:
        next_position = haystack.find(needle, position + 1)
        if next_position < 0:
            fail(f"{context}: missing ordered literal {needle!r}")
        position = next_position


def assert_call_order(haystack: str, names: list[str], context: str) -> None:
    pattern = r"VM_TryLoadNativeModule\s*\(\s*\"([^\"]+)\""
    calls = re.findall(pattern, haystack)
    search_from = 0
    for expected in names:
        try:
            found_at = calls.index(expected, search_from)
        except ValueError:
            fail(f"{context}: expected VM_TryLoadNativeModule(\"{expected}\") after {calls[:search_from]}")
        search_from = found_at + 1


vm_text = read(vm_path)
files_text = read(files_path)

load_native = find_function_body(vm_text, "loadNative")
vm_call = find_function_body(vm_text, "VM_Call")
fs_startup = find_function_body(files_text, "FS_Startup")
pk3_cache = find_function_body(files_text, "FS_TryLoadLibraryFromPk3Cache")
fs_load_library = find_function_body(files_text, "FS_LoadLibrary")

# VM native modules are the ABI bridge for classic and renamed game modules.
assert_contains(load_native, 'Q_stricmp( name, "qagame" ) == 0', "loadNative generic qagame alias")
assert_contains(load_native, 'Q_stricmp( name, "frontend" ) == 0', "loadNative frontend alias")
assert_call_order(load_native, ["game", "server", "client", "frontend", "game", "cgame", "ui"], "loadNative alias probe order")
assert_order(
    load_native,
    [
        'Q_stricmp( name, "qagame" ) == 0',
        'VM_TryLoadNativeModule( "game", filename, sizeof( filename ) )',
        'VM_TryLoadNativeModule( "server", filename, sizeof( filename ) )',
    ],
    "qagame native alias order",
)
assert_order(
    load_native,
    [
        'Q_stricmp( name, "cgame" ) == 0',
        'VM_TryLoadNativeModule( "client", filename, sizeof( filename ) )',
    ],
    "cgame native alias order",
)
assert_order(
    load_native,
    [
        'Q_stricmp( name, "ui" ) == 0',
        'VM_TryLoadNativeModule( "frontend", filename, sizeof( filename ) )',
    ],
    "ui native alias order",
)
assert_order(
    load_native,
    [
        'Q_stricmp( name, "server" ) == 0',
        'VM_TryLoadNativeModule( "game", filename, sizeof( filename ) )',
    ],
    "server reverse alias order",
)
assert_order(
    load_native,
    [
        'Q_stricmp( name, "client" ) == 0',
        'VM_TryLoadNativeModule( "cgame", filename, sizeof( filename ) )',
    ],
    "client reverse alias order",
)
assert_order(
    load_native,
    [
        'Q_stricmp( name, "frontend" ) == 0',
        'VM_TryLoadNativeModule( "ui", filename, sizeof( filename ) )',
    ],
    "frontend reverse alias order",
)

# Native vmMain receives fixed arg slots; omitted varargs must not leak stack garbage.
assert_contains(vm_call, "int32_t args[MAX_VMMAIN_CALL_ARGS-1];", "VM_Call native args storage")
assert_contains(vm_call, "Com_Memset( args, 0, sizeof( args ) );", "VM_Call native zero fill")
assert_regex(vm_call, r"for\s*\(\s*i\s*=\s*0\s*;\s*i\s*<\s*nargs\s*;\s*i\+\+\s*\)", "VM_Call native vararg loop bound")
assert_contains(vm_call, "args[i] = va_arg( ap, int32_t );", "VM_Call native vararg copy")
assert_contains(vm_call, "r = vm->entryPoint( callnum, args[0], args[1], args[2] );", "VM_Call native entry dispatch")
assert_order(
    vm_call,
    [
        "Com_Memset( args, 0, sizeof( args ) );",
        "args[i] = va_arg( ap, int32_t );",
        "r = vm->entryPoint( callnum, args[0], args[1], args[2] );",
    ],
    "VM_Call native zero-fill before dispatch",
)

# pk3 native extraction is default-on, archived, documented, and visible at startup.
assert_contains(fs_startup, 'Cvar_Get( "com_nativeLibraryExtractPk3", "1", CVAR_ARCHIVE )', "pk3 extraction cvar registration")
assert_contains(fs_startup, "Cvar_SetDescription( com_nativeLibraryExtractPk3,", "pk3 extraction cvar description")
assert_contains(fs_startup, "extracting embedded native libs from pk3 is enabled", "pk3 extraction startup log")

# Cache extraction must be gated, basename-only, extension-filtered, and never create fallbacks for explicit paths.
assert_contains(pk3_cache, "if ( !com_nativeLibraryExtractPk3 || !com_nativeLibraryExtractPk3->integer )", "pk3 cache cvar gate")
assert_contains(pk3_cache, "if ( !name || !name[0] )", "pk3 cache empty-name guard")
assert_contains(pk3_cache, "base = slash + 1;", "pk3 cache basename extraction")
assert_contains(pk3_cache, 'strstr( base, ".dll" )', "pk3 cache dll filter")
assert_contains(pk3_cache, 'Q_stricmp( base + strlen( base ) - 3, ".so" )', "pk3 cache so filter")
assert_contains(pk3_cache, 'Com_sprintf( cacheQpath, sizeof( cacheQpath ), "%s%s", FS_NATIVE_LIB_CACHE_PREFIX, base );', "pk3 cache basename qpath")
assert_contains(pk3_cache, 'len = FS_ReadFile( name, &fileBuf );', "pk3 cache direct read")
assert_order(
    pk3_cache,
    [
        'len = FS_ReadFile( name, &fileBuf );',
        "if ( slash ) {",
        "return NULL;",
        'Com_sprintf( alt, sizeof( alt ), "vm/%s", name );',
        'len = FS_ReadFile( alt, &fileBuf );',
        'Com_sprintf( alt, sizeof( alt ), "modules/%s", name );',
        'len = FS_ReadFile( alt, &fileBuf );',
    ],
    "pk3 cache direct/vm/modules fallback order",
)
if pk3_cache.count("FS_ReadFile(") != 3:
    fail(f"pk3 cache read attempts: expected exactly 3, found {pk3_cache.count('FS_ReadFile(')}")

# Reusing an extracted native library must prove the cached bytes match the pk3 bytes first.
assert_order(
    pk3_cache,
    [
        "crcPak = crc32_buffer",
        'fp = Sys_FOpen( osCachePath, "rb" );',
        "readLen == len",
        "crcDisk = crc32_buffer",
        "if ( crcDisk == crcPak )",
        "h = FS_TryLoadLibraryPath( osCachePath );",
    ],
    "pk3 cache CRC reuse gate",
)
assert_contains(pk3_cache, "FS_FOpenFileWrite rejects .so paths", "pk3 cache direct-write rationale")
assert_contains(pk3_cache, 'Sys_FOpen( osCachePath, "wb" )', "pk3 cache direct file write")
assert_contains(pk3_cache, "fwrite( fileBuf, 1, (size_t)len, out )", "pk3 cache byte write")
assert_not_contains(strip_comments(pk3_cache), "FS_WriteFile", "pk3 cache must not use filesystem write filter")
assert_contains(pk3_cache, "failed to write pk3 native cache", "pk3 cache write warning")
assert_contains(pk3_cache, "extracted pk3 native lib to", "pk3 cache extraction log")

# The pk3 cache must run before loose filesystem probing so pk3-only native modules can load.
assert_order(
    fs_load_library,
    [
        "libHandle = FS_TryLoadLibraryFromPk3Cache( name );",
        "while ( !libHandle && sp )",
        'Com_sprintf( vmPath, sizeof( vmPath ), "modules/%s", name );',
        'Com_sprintf( vmPath, sizeof( vmPath ), "vm/%s", name );',
    ],
    "FS_LoadLibrary pk3 cache before loose paths",
)

print("PASS: test_native_module_regressions")
PY
