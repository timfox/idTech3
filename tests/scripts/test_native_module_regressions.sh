#!/usr/bin/env bash
# Regression checks for native VM calls and pk3-backed native module loading.
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
vm_c = root / "src/qcommon/vm.c"
files_c = root / "src/qcommon/files.c"


def fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)
    sys.exit(1)


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//.*", "", text)


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
    first_pos = haystack.find(first)
    second_pos = haystack.find(second)
    if first_pos < 0 or second_pos < 0 or first_pos >= second_pos:
        fail(f"{context}: expected {first!r} before {second!r}")


def assert_count(haystack: str, needle: str, expected: int, context: str) -> None:
    count = haystack.count(needle)
    if count != expected:
        fail(f"{context}: expected {expected} occurrences of {needle!r}, found {count}")


for path in (vm_c, files_c):
    if not path.is_file():
        fail(f"missing source file: {path}")

vm_text = vm_c.read_text()
files_text = files_c.read_text()

vm_call = find_function_body(vm_text, "VM_Call")
vm_context = "src/qcommon/vm.c:VM_Call"
assert_contains(vm_call, "int32_t args[MAX_VMMAIN_CALL_ARGS-1];", vm_context)
assert_contains(vm_call, "Com_Memset( args, 0, sizeof( args ) );", vm_context)
assert_regex(
    vm_call,
    r"for\s*\(\s*i\s*=\s*0\s*;\s*i\s*<\s*nargs\s*;\s*i\+\+\s*\)\s*\{.*?"
    r"args\s*\[\s*i\s*\]\s*=\s*va_arg\s*\(\s*ap\s*,\s*int32_t\s*\)\s*;",
    vm_context,
)
assert_contains(vm_call, "r = vm->entryPoint( callnum, args[0], args[1], args[2] );", vm_context)
assert_order(vm_call, "Com_Memset( args, 0, sizeof( args ) );", "va_start( ap, callnum );", vm_context)
assert_order(vm_call, "va_start( ap, callnum );", "r = vm->entryPoint( callnum, args[0], args[1], args[2] );", vm_context)

files_context = "src/qcommon/files.c"
assert_contains(
    files_text,
    'com_nativeLibraryExtractPk3 = Cvar_Get( "com_nativeLibraryExtractPk3", "1", CVAR_ARCHIVE );',
    f"{files_context}:com_nativeLibraryExtractPk3 registration",
)
assert_contains(
    files_text,
    "Cvar_SetDescription( com_nativeLibraryExtractPk3,",
    f"{files_context}:com_nativeLibraryExtractPk3 description",
)
assert_contains(
    files_text,
    'Com_Printf( "com_nativeLibraryExtractPk3: extracting embedded native libs from pk3 is enabled.\\n" );',
    f"{files_context}:com_nativeLibraryExtractPk3 startup log",
)

pk3_body = find_function_body(files_text, "FS_TryLoadLibraryFromPk3Cache")
pk3_context = "src/qcommon/files.c:FS_TryLoadLibraryFromPk3Cache"
uncommented_pk3_body = strip_comments(pk3_body)

assert_contains(pk3_body, "if ( !com_nativeLibraryExtractPk3 || !com_nativeLibraryExtractPk3->integer )", pk3_context)
assert_contains(pk3_body, "if ( !name || !name[0] )", pk3_context)
assert_contains(pk3_body, "base = name;", pk3_context)
assert_contains(pk3_body, "slash = strrchr( name, '/' );", pk3_context)
assert_contains(pk3_body, "base = slash + 1;", pk3_context)
assert_contains(pk3_body, 'strstr( base, ".dll" )', pk3_context)
assert_contains(pk3_body, 'Q_stricmp( base + strlen( base ) - 3, ".so" )', pk3_context)
assert_contains(pk3_body, 'Com_sprintf( cacheQpath, sizeof( cacheQpath ), "%s%s", FS_NATIVE_LIB_CACHE_PREFIX, base );', pk3_context)
assert_contains(pk3_body, "len = FS_ReadFile( name, &fileBuf );", pk3_context)
assert_contains(pk3_body, "if ( slash ) {\n\t\t\treturn NULL;\n\t\t}", pk3_context)
assert_contains(pk3_body, 'Com_sprintf( alt, sizeof( alt ), "vm/%s", name );', pk3_context)
assert_contains(pk3_body, 'Com_sprintf( alt, sizeof( alt ), "modules/%s", name );', pk3_context)
assert_order(pk3_body, 'Com_sprintf( alt, sizeof( alt ), "vm/%s", name );', 'Com_sprintf( alt, sizeof( alt ), "modules/%s", name );', pk3_context)
assert_count(pk3_body, "FS_ReadFile(", 3, pk3_context)
assert_contains(pk3_body, "crcPak = crc32_buffer( (const byte *)fileBuf, (unsigned int)len );", pk3_context)
assert_contains(pk3_body, "FS_BuildOSPath( fs_homepath->string, fs_gamedir, cacheQpath )", pk3_context)
assert_contains(pk3_body, 'fp = Sys_FOpen( osCachePath, "rb" );', pk3_context)
assert_contains(pk3_body, "readLen > 0 && readLen == len", pk3_context)
assert_contains(pk3_body, "crcDisk = crc32_buffer( (const byte *)diskBuf, (unsigned int)readLen );", pk3_context)
assert_order(pk3_body, "if ( crcDisk == crcPak )", "h = FS_TryLoadLibraryPath( osCachePath );", pk3_context)
assert_contains(pk3_body, "FS_CreatePath( osCachePath )", pk3_context)
assert_contains(pk3_body, 'FILE *out = Sys_FOpen( osCachePath, "wb" );', pk3_context)
assert_contains(pk3_body, "fwrite( fileBuf, 1, (size_t)len, out )", pk3_context)
assert_not_contains(uncommented_pk3_body, "FS_WriteFile(", pk3_context)
assert_contains(pk3_body, 'Com_Printf( S_COLOR_YELLOW "FS_LoadLibrary: failed to write pk3 native cache %s\\n", osCachePath );', pk3_context)
assert_contains(pk3_body, 'Com_Printf( "FS_LoadLibrary: using pk3 native cache %s\\n", cacheQpath );', pk3_context)
assert_contains(pk3_body, 'Com_Printf( "FS_LoadLibrary: extracted pk3 native lib to %s\\n", cacheQpath );', pk3_context)

load_body = find_function_body(files_text, "FS_LoadLibrary")
load_context = "src/qcommon/files.c:FS_LoadLibrary"
assert_contains(load_body, "libHandle = FS_TryLoadLibraryFromPk3Cache( name );", load_context)
assert_order(load_body, "libHandle = FS_TryLoadLibraryFromPk3Cache( name );", "while ( !libHandle && sp )", load_context)

print("PASS: test_native_module_regressions")
PY
