#!/usr/bin/env bash
# Guard pk3-backed native module extraction and loader ordering.
# The path is hard to exercise in headless CI because it requires a valid native game module
# inside a .pk3; these source invariants catch regressions in the high-risk filesystem logic.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$SCRIPT_DIR/idtech3_test_paths.sh"
idtech3_test_paths_init "$PROJECT_ROOT"
FILES_C="$(idtech3_require_file engine/core/files.c src/qcommon/files.c)"

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

assert_contains_literal() {
	local haystack="$1"
	local literal="$2"
	local context="$3"
	if [[ "$haystack" != *"$literal"* ]]; then
		fail "$context: expected literal '$literal'"
	fi
}

assert_count_literal() {
	local haystack="$1"
	local literal="$2"
	local expected="$3"
	local context="$4"
	local count
	count="$(HAYSTACK="$haystack" LITERAL="$literal" python3 - <<'PY'
import os
print(os.environ["HAYSTACK"].count(os.environ["LITERAL"]))
PY
)"
	if [[ "$count" != "$expected" ]]; then
		fail "$context: expected $expected occurrence(s) of '$literal', found $count"
	fi
}

extract_function() {
	local function_name="$1"
	python3 - "$FILES_C" "$function_name" <<'PY'
import sys
from pathlib import Path

path = Path(sys.argv[1])
name = sys.argv[2]
text = path.read_text()
needle = f"{name}("
start = -1
search_from = 0
while True:
    candidate = text.find(needle, search_from)
    if candidate < 0:
        break
    line_start = text.rfind("\n", 0, candidate) + 1
    prefix = text[line_start:candidate]
    if not prefix.lstrip().startswith("*") and "Called " not in prefix:
        start = candidate
        break
    search_from = candidate + len(needle)
if start < 0:
    raise SystemExit(f"missing function: {name}")
brace = text.find("{", start)
if brace < 0:
    raise SystemExit(f"missing function body: {name}")
depth = 0
for index in range(brace, len(text)):
    char = text[index]
    if char == "{":
        depth += 1
    elif char == "}":
        depth -= 1
        if depth == 0:
            print(text[start:index + 1])
            raise SystemExit(0)
raise SystemExit(f"unterminated function body: {name}")
PY
}

assert_order() {
	local haystack="$1"
	local before="$2"
	local after="$3"
	local context="$4"
	HAYSTACK="$haystack" BEFORE="$before" AFTER="$after" CONTEXT="$context" python3 - <<'PY'
import os
import sys

haystack = os.environ["HAYSTACK"]
before = os.environ["BEFORE"]
after = os.environ["AFTER"]
context = os.environ["CONTEXT"]
before_index = haystack.find(before)
after_index = haystack.find(after)
if before_index < 0 or after_index < 0 or before_index >= after_index:
    print(f"FAIL: {context}: expected '{before}' before '{after}'", file=sys.stderr)
    sys.exit(1)
PY
}

[ -f "$FILES_C" ] || fail "missing file: $FILES_C"
command -v python3 >/dev/null 2>&1 || fail "python3 not in PATH"

startup_body="$(extract_function FS_Startup)"
cache_body="$(extract_function FS_TryLoadLibraryFromPk3Cache)"
load_body="$(extract_function FS_LoadLibrary)"
files_source="$(<"$FILES_C")"

# The feature must remain archived/default-on and visible in startup logs for supportability.
assert_contains_literal "$startup_body" 'com_nativeLibraryExtractPk3 = Cvar_Get( "com_nativeLibraryExtractPk3", "1", CVAR_ARCHIVE );' "pk3 extraction cvar registration"
assert_contains_literal "$startup_body" 'Cvar_SetDescription( com_nativeLibraryExtractPk3,' "pk3 extraction cvar description"
assert_contains_literal "$startup_body" 'Com_Printf( "com_nativeLibraryExtractPk3: extracting embedded native libs from pk3 is enabled.\n" );' "pk3 extraction startup log"

# Extraction must be explicitly gated, extension-limited, and basename-scoped to avoid unsafe writes.
assert_contains_literal "$cache_body" 'if ( !com_nativeLibraryExtractPk3 || !com_nativeLibraryExtractPk3->integer ) {' "pk3 extraction cvar gate"
assert_contains_literal "$cache_body" 'return NULL;' "pk3 extraction early returns"
assert_contains_literal "$cache_body" 'base = slash + 1;' "pk3 cache basename extraction"
assert_contains_literal "$files_source" '#define FS_NATIVE_LIB_CACHE_PREFIX "vm/native_cache/"' "pk3 native cache directory"
assert_contains_literal "$cache_body" 'Com_sprintf( cacheQpath, sizeof( cacheQpath ), "%s%s", FS_NATIVE_LIB_CACHE_PREFIX, base );' "pk3 native cache qpath"
assert_contains_literal "$cache_body" 'FS_BuildOSPath( fs_homepath->string, fs_gamedir, cacheQpath )' "pk3 native cache homepath location"
assert_contains_literal "$cache_body" 'Q_stricmp( base + strlen( base ) - 3, ".so" ) != 0' "non-Windows native extension guard"
assert_contains_literal "$cache_body" '!strstr( base, ".dll" )' "Windows native extension guard"

# The pk3 lookup order must still support legacy direct names, vm/, then modules/.
assert_order "$cache_body" 'len = FS_ReadFile( name, &fileBuf );' 'Com_sprintf( alt, sizeof( alt ), "vm/%s", name );' "direct name before vm fallback"
assert_order "$cache_body" 'Com_sprintf( alt, sizeof( alt ), "vm/%s", name );' 'Com_sprintf( alt, sizeof( alt ), "modules/%s", name );' "vm fallback before modules fallback"
assert_count_literal "$cache_body" 'FS_ReadFile(' 3 "pk3 native library read fallbacks"

# Existing cache files may only be reused when length and CRC match the pk3 payload.
assert_contains_literal "$cache_body" 'crcPak = crc32_buffer( (const byte *)fileBuf, (unsigned int)len );' "pk3 payload CRC"
assert_contains_literal "$cache_body" 'readLen > 0 && readLen == len' "pk3 cache length match"
assert_contains_literal "$cache_body" 'crcDisk = crc32_buffer( (const byte *)diskBuf, (unsigned int)readLen );' "pk3 cache disk CRC"
assert_contains_literal "$cache_body" 'if ( crcDisk == crcPak ) {' "pk3 cache CRC gate"
assert_order "$cache_body" 'if ( crcDisk == crcPak ) {' 'h = FS_TryLoadLibraryPath( osCachePath );' "cache load after CRC gate"

# The write path intentionally bypasses FS_FOpenFileWrite because native library extensions are denied.
assert_contains_literal "$cache_body" '/* FS_FOpenFileWrite rejects .so paths; write cache file directly (see FS_CheckFilenameIsNotAllowed). */' "pk3 cache write rationale"
assert_contains_literal "$cache_body" 'if ( FS_CreatePath( osCachePath ) ) {' "pk3 cache directory creation"
assert_contains_literal "$cache_body" 'FILE *out = Sys_FOpen( osCachePath, "wb" );' "pk3 cache direct write"
assert_contains_literal "$cache_body" 'Com_Printf( S_COLOR_YELLOW "FS_LoadLibrary: failed to write pk3 native cache %s\n", osCachePath );' "pk3 cache write warning"
assert_contains_literal "$cache_body" 'Com_Printf( "FS_LoadLibrary: extracted pk3 native lib to %s\n", cacheQpath );' "pk3 cache extraction log"

# FS_LoadLibrary must try the pk3 cache before loose filesystem searchpaths.
assert_order "$load_body" 'libHandle = FS_TryLoadLibraryFromPk3Cache( name );' 'while ( !libHandle && sp ) {' "pk3 cache attempted before loose modules"
assert_contains_literal "$load_body" 'return libHandle;' "FS_LoadLibrary returns pk3 cache handle"

echo "PASS: test_pk3_native_library_cache"
