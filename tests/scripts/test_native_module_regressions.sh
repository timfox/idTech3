#!/usr/bin/env bash
# Regression checks for native VM module loading fixes.
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


def assert_regex(haystack: str, pattern: str, context: str) -> None:
    if not re.search(pattern, haystack, flags=re.S):
        fail(f"{context}: expected pattern {pattern!r}")


def assert_before(haystack: str, first: str, second: str, context: str) -> None:
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
if not vm_c.is_file():
    fail(f"missing source file: {vm_c}")

files_text = files_c.read_text()
files_no_comments = strip_comments(files_text)

assert_contains(files_text, "static\tcvar_t\t\t*com_nativeLibraryExtractPk3;", "files.c cvar declaration")
assert_regex(
    files_text,
    r'com_nativeLibraryExtractPk3\s*=\s*Cvar_Get\s*\(\s*"com_nativeLibraryExtractPk3"\s*,\s*"1"\s*,\s*CVAR_ARCHIVE\s*\)',
    "com_nativeLibraryExtractPk3 default",
)
assert_contains(files_text, "Cvar_SetDescription( com_nativeLibraryExtractPk3,", "com_nativeLibraryExtractPk3 description")
assert_contains(files_text, "vm/native_cache/", "com_nativeLibraryExtractPk3 cache description")
assert_contains(
    files_text,
    'Com_Printf( "com_nativeLibraryExtractPk3: extracting embedded native libs from pk3 is enabled.\\n" );',
    "com_nativeLibraryExtractPk3 startup log",
)

cache_body = find_function_body(files_text, "FS_TryLoadLibraryFromPk3Cache")
cache_no_comments = strip_comments(cache_body)
context = "FS_TryLoadLibraryFromPk3Cache"

assert_contains(files_text, "FS_NATIVE_LIB_CACHE_PREFIX \"vm/native_cache/\"", context)
assert_regex(
    cache_body,
    r"if\s*\(\s*!com_nativeLibraryExtractPk3\s*\|\|\s*!com_nativeLibraryExtractPk3->integer\s*\)\s*\{\s*return NULL;",
    f"{context} cvar gate",
)
assert_regex(cache_body, r"slash\s*=\s*strrchr\s*\(\s*name\s*,\s*'/'\s*\)", f"{context} basename slash")
assert_contains(cache_body, "base = slash + 1;", f"{context} basename selection")
assert_contains(cache_body, 'Com_sprintf( cacheQpath, sizeof( cacheQpath ), "%s%s", FS_NATIVE_LIB_CACHE_PREFIX, base );', f"{context} cache qpath")
assert_regex(cache_body, r"Q_stricmp\s*\(\s*base\s*\+\s*strlen\s*\(\s*base\s*\)\s*-\s*3\s*,\s*\"\.so\"\s*\)", f"{context} unix extension guard")
assert_contains(cache_body, 'strstr( base, ".dll" )', f"{context} windows extension guard")

read_calls = re.findall(r"\bFS_ReadFile\s*\(", cache_no_comments)
if len(read_calls) != 3:
    fail(f"{context}: expected exactly three pk3 read probes, found {len(read_calls)}")
assert_before(cache_body, "len = FS_ReadFile( name, &fileBuf );", 'Com_sprintf( alt, sizeof( alt ), "vm/%s", name );', f"{context} direct before vm fallback")
assert_before(cache_body, 'Com_sprintf( alt, sizeof( alt ), "vm/%s", name );', 'Com_sprintf( alt, sizeof( alt ), "modules/%s", name );', f"{context} vm before modules fallback")
assert_regex(
    cache_body,
    r"if\s*\(\s*slash\s*\)\s*\{\s*return NULL;\s*\}\s*Com_sprintf\s*\(\s*alt\s*,\s*sizeof\s*\(\s*alt\s*\)\s*,\s*\"vm/%s\"",
    f"{context} no path fallback for explicit paths",
)

assert_contains(cache_body, "crcPak = crc32_buffer( (const byte *)fileBuf, (unsigned int)len );", f"{context} pk3 crc")
assert_contains(cache_body, "Q_strncpyz( osCachePath, FS_BuildOSPath( fs_homepath->string, fs_gamedir, cacheQpath ), sizeof( osCachePath ) );", f"{context} homepath cache path")
assert_contains(cache_body, "crcDisk = crc32_buffer( (const byte *)diskBuf, (unsigned int)readLen );", f"{context} disk crc")
assert_before(cache_body, "if ( crcDisk == crcPak )", "h = FS_TryLoadLibraryPath( osCachePath );", f"{context} crc before cached load")
assert_contains(cache_body, 'Com_Printf( "FS_LoadLibrary: using pk3 native cache %s\\n", cacheQpath );', f"{context} cache reuse log")

if re.search(r"\bFS_WriteFile\s*\(", cache_no_comments):
    fail(f"{context}: .so cache writes must not use FS_WriteFile")
assert_contains(cache_body, "if ( FS_CreatePath( osCachePath ) )", f"{context} create cache directory")
assert_contains(cache_body, 'FILE *out = Sys_FOpen( osCachePath, "wb" );', f"{context} direct binary write")
assert_contains(cache_body, "fwrite( fileBuf, 1, (size_t)len, out ) != (size_t)len", f"{context} full write check")
assert_contains(cache_body, 'Com_Printf( S_COLOR_YELLOW "FS_LoadLibrary: failed to write pk3 native cache %s\\n", osCachePath );', f"{context} write failure warning")
assert_before(cache_body, "FS_FreeFile( fileBuf );", "h = FS_TryLoadLibraryPath( osCachePath );", f"{context} extracted load after buffer free")
assert_contains(cache_body, 'Com_Printf( "FS_LoadLibrary: extracted pk3 native lib to %s\\n", cacheQpath );', f"{context} extraction log")

load_body = find_function_body(files_text, "FS_LoadLibrary")
assert_before(load_body, "libHandle = FS_TryLoadLibraryFromPk3Cache( name );", "while ( !libHandle && sp )", "FS_LoadLibrary pk3 cache before loose paths")
assert_regex(load_body, r"libHandle\s*=\s*FS_TryLoadLibraryFromPk3Cache\s*\(\s*name\s*\)\s*;\s*if\s*\(\s*libHandle\s*\)\s*\{\s*return libHandle;", "FS_LoadLibrary early pk3 return")

vm_text = vm_c.read_text()
vm_call_body = find_function_body(vm_text, "VM_Call")
vm_context = "VM_Call native entryPoint"

assert_contains(vm_call_body, "if ( vm->entryPoint )", vm_context)
assert_contains(vm_call_body, "int32_t args[MAX_VMMAIN_CALL_ARGS-1];", vm_context)
assert_contains(vm_call_body, "Com_Memset( args, 0, sizeof( args ) );", f"{vm_context} zero-fill")
assert_before(vm_call_body, "Com_Memset( args, 0, sizeof( args ) );", "va_start( ap, callnum );", f"{vm_context} zero before varargs")
assert_before(vm_call_body, "for ( i = 0; i < nargs; i++ )", "r = vm->entryPoint( callnum, args[0], args[1], args[2] );", f"{vm_context} pack before call")
assert_contains(vm_call_body, "args[i] = va_arg( ap, int32_t );", f"{vm_context} vararg copy")
assert_contains(vm_call_body, "r = vm->entryPoint( callnum, args[0], args[1], args[2] );", f"{vm_context} fixed native arity")

print("PASS: test_native_module_regressions")
PY
