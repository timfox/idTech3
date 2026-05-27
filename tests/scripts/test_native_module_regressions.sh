#!/usr/bin/env bash
# Regression checks for native VM module loading and pk3 extraction invariants.
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
vm_c = root / "src/qcommon/vm.c"


def fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)
    sys.exit(1)


def read(path: Path) -> str:
    if not path.is_file():
        fail(f"missing source file: {path}")
    return path.read_text()


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


def assert_not_contains(haystack: str, needle: str, context: str) -> None:
    if needle in haystack:
        fail(f"{context}: unexpected {needle!r}")


def assert_regex(haystack: str, pattern: str, context: str) -> None:
    if not re.search(pattern, haystack, flags=re.S):
        fail(f"{context}: expected pattern {pattern!r}")


def assert_order(haystack: str, first: str, second: str, context: str) -> None:
    first_idx = haystack.find(first)
    second_idx = haystack.find(second)
    if first_idx < 0 or second_idx < 0 or first_idx >= second_idx:
        fail(f"{context}: expected {first!r} before {second!r}")


files_text = read(files_c)
vm_text = read(vm_c)

assert_contains(
    files_text,
    'com_nativeLibraryExtractPk3 = Cvar_Get( "com_nativeLibraryExtractPk3", "1", CVAR_ARCHIVE );',
    "native pk3 extraction cvar should remain archived and default-on",
)
assert_contains(
    files_text,
    "Cvar_SetDescription( com_nativeLibraryExtractPk3,",
    "native pk3 extraction cvar should keep a runtime description",
)
assert_contains(
    files_text,
    'Com_Printf( "com_nativeLibraryExtractPk3: extracting embedded native libs from pk3 is enabled.\\n" );',
    "native pk3 extraction should keep a startup log",
)

pk3_body = find_function_body(files_text, "FS_TryLoadLibraryFromPk3Cache")
assert_contains(
    pk3_body,
    "if ( !com_nativeLibraryExtractPk3 || !com_nativeLibraryExtractPk3->integer )",
    "pk3 cache loader must honor the cvar kill switch",
)
assert_contains(pk3_body, "#if defined( _WIN32 )", "pk3 cache loader must keep Windows extension gate")
assert_contains(pk3_body, 'strstr( base, ".dll" )', "pk3 cache loader must accept Windows DLLs")
assert_contains(pk3_body, 'Q_stricmp( base + strlen( base ) - 3, ".so" )', "pk3 cache loader must accept Unix shared objects")
assert_contains(
    pk3_body,
    'Com_sprintf( cacheQpath, sizeof( cacheQpath ), "%s%s", FS_NATIVE_LIB_CACHE_PREFIX, base );',
    "pk3 cache loader must cache by basename only",
)
assert_contains(pk3_body, "len = FS_ReadFile( name, &fileBuf );", "pk3 cache loader must try the requested qpath first")
assert_order(
    pk3_body,
    "if ( slash ) {\n\t\t\treturn NULL;\n\t\t}",
    'Com_sprintf( alt, sizeof( alt ), "vm/%s", name );',
    "explicit native-library paths must not fall back to vm/modules aliases",
)
assert_order(
    pk3_body,
    'Com_sprintf( alt, sizeof( alt ), "vm/%s", name );',
    'Com_sprintf( alt, sizeof( alt ), "modules/%s", name );',
    "pk3 cache loader must probe vm/ before modules/ for bare names",
)
assert_contains(pk3_body, "crcPak = crc32_buffer( (const byte *)fileBuf, (unsigned int)len );", "pk3 bytes must be checksummed")
assert_contains(
    pk3_body,
    "Q_strncpyz( osCachePath, FS_BuildOSPath( fs_homepath->string, fs_gamedir, cacheQpath ), sizeof( osCachePath ) );",
    "pk3 cache must write under fs_homepath/fs_game",
)
assert_contains(pk3_body, 'fp = Sys_FOpen( osCachePath, "rb" );', "pk3 cache loader must inspect an existing cache file")
assert_regex(pk3_body, r"readLen\s*>\s*0\s*&&\s*readLen\s*==\s*len", "pk3 cache reuse must require a matching length")
assert_contains(pk3_body, "crcDisk = crc32_buffer( (const byte *)diskBuf, (unsigned int)readLen );", "pk3 cache reuse must checksum the disk copy")
assert_order(
    pk3_body,
    "if ( crcDisk == crcPak )",
    "h = FS_TryLoadLibraryPath( osCachePath );",
    "pk3 cache reuse must only dlopen after CRC validation",
)
assert_contains(pk3_body, 'FILE *out = Sys_FOpen( osCachePath, "wb" );', "pk3 cache writes must use direct OS file I/O")
assert_contains(pk3_body, "fwrite( fileBuf, 1, (size_t)len, out )", "pk3 cache writes must persist the exact pk3 bytes")
assert_not_contains(pk3_body, "FS_WriteFile", "pk3 cache must not route native-library writes through qpath write filters")
assert_contains(pk3_body, "FS_LoadLibrary: using pk3 native cache", "pk3 cache reuse should remain diagnosable")
assert_contains(pk3_body, "FS_LoadLibrary: extracted pk3 native lib", "pk3 extraction should remain diagnosable")

load_body = find_function_body(files_text, "FS_LoadLibrary")
assert_order(
    load_body,
    "libHandle = FS_TryLoadLibraryFromPk3Cache( name );",
    "const searchpath_t *sp = fs_searchpaths;",
    "FS_LoadLibrary must try pk3-backed native modules before loose filesystem search paths",
)
assert_order(
    load_body,
    "libHandle = FS_TryLoadLibraryFromPk3Cache( name );",
    "while ( !libHandle && sp )",
    "FS_LoadLibrary must keep pk3 cache first in the load order",
)

vmcall_body = find_function_body(vm_text, "VM_Call")
assert_contains(
    vmcall_body,
    "int32_t args[MAX_VMMAIN_CALL_ARGS-1];",
    "native VM_Call must keep fixed native argument slots",
)
assert_contains(
    vmcall_body,
    "Com_Memset( args, 0, sizeof( args ) );",
    "native VM_Call must zero-fill omitted vmMain args",
)
assert_order(
    vmcall_body,
    "Com_Memset( args, 0, sizeof( args ) );",
    "va_start( ap, callnum );",
    "native VM_Call must zero args before copying provided varargs",
)
assert_regex(
    vmcall_body,
    r"for\s*\(\s*i\s*=\s*0;\s*i\s*<\s*nargs;\s*i\+\+\s*\)\s*\{[^}]*args\[i\]\s*=\s*va_arg\s*\(\s*ap,\s*int32_t\s*\);",
    "native VM_Call must copy only caller-provided args",
)
assert_contains(
    vmcall_body,
    "r = vm->entryPoint( callnum, args[0], args[1], args[2] );",
    "native VM_Call must pass three stable vmMain slots",
)

load_native_body = find_function_body(vm_text, "loadNative")
assert_order(load_native_body, 'Q_stricmp( name, "qagame" ) == 0', 'VM_TryLoadNativeModule( "game"', "qagame must keep legacy game alias fallback")
assert_order(load_native_body, 'VM_TryLoadNativeModule( "game"', 'VM_TryLoadNativeModule( "server"', "qagame/game must try legacy game alias before project server alias")
assert_contains(load_native_body, 'VM_TryLoadNativeModule( "client"', "cgame must keep project client alias fallback")
assert_contains(load_native_body, 'VM_TryLoadNativeModule( "frontend"', "ui must keep project frontend alias fallback")
assert_contains(load_native_body, 'VM_TryLoadNativeModule( "cgame"', "client must keep reverse cgame alias fallback")
assert_contains(load_native_body, 'VM_TryLoadNativeModule( "ui"', "frontend must keep reverse ui alias fallback")

print("PASS: test_native_module_regressions")
PY
