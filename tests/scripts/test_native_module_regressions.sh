#!/usr/bin/env bash
# Regression checks for native VM calls and pk3-backed native library loading.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
VM_SOURCE="$PROJECT_ROOT/src/qcommon/vm.c"
FILES_SOURCE="$PROJECT_ROOT/src/qcommon/files.c"

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

assert_contains() {
	local haystack="$1"
	local needle="$2"
	local context="$3"
	if [[ "$haystack" != *"$needle"* ]]; then
		fail "$context: expected to find '$needle'"
	fi
}

assert_not_contains() {
	local haystack="$1"
	local needle="$2"
	local context="$3"
	if [[ "$haystack" == *"$needle"* ]]; then
		fail "$context: unexpected '$needle'"
	fi
}

assert_order() {
	local haystack="$1"
	local first="$2"
	local second="$3"
	local context="$4"
	local order

	order="$(FIRST="$first" SECOND="$second" awk '
		BEGIN {
			a = ENVIRON["FIRST"];
			b = ENVIRON["SECOND"];
			first_line = 0;
			second_line = 0;
		}
		{
			if (first_line == 0 && index($0, a) > 0) {
				first_line = NR;
			}
			if (second_line == 0 && index($0, b) > 0) {
				second_line = NR;
			}
		}
		END {
			if (first_line > 0 && second_line > 0 && first_line < second_line) {
				print "ok";
			} else {
				printf("bad:%d:%d\n", first_line, second_line);
			}
		}
	' <<<"$haystack")"
	if [[ "$order" != "ok" ]]; then
		fail "$context: expected '$first' before '$second' ($order)"
	fi
}

extract_function() {
	local file="$1"
	local name="$2"

	awk -v name="$name" '
		BEGIN {
			def = "^[[:space:]]*(static[[:space:]]+)?[A-Za-z_][A-Za-z0-9_[:space:]\\*]*" name "[[:space:]]*\\(";
		}
		$0 ~ def && index($0, "{") > 0 && in_fn == 0 {
			in_fn = 1;
		}
		in_fn {
			line = $0;
			print line;
			opens += gsub(/\{/, "{", line);
			closes += gsub(/\}/, "}", line);
			if (opens > 0 && opens == closes) {
				exit;
			}
		}
	' "$file"
}

extract_until() {
	local file="$1"
	local start="$2"
	local stop="$3"

	awk -v start="$start" -v stop="$stop" '
		index($0, start) > 0 && in_block == 0 {
			in_block = 1;
		}
		in_block {
			if (index($0, stop) > 0) {
				exit;
			}
			print;
		}
	' "$file"
}

if [[ ! -f "$VM_SOURCE" ]]; then
	fail "missing VM source: $VM_SOURCE"
fi
if [[ ! -f "$FILES_SOURCE" ]]; then
	fail "missing filesystem source: $FILES_SOURCE"
fi

vm_native_body="$(extract_until "$VM_SOURCE" "if ( vm->entryPoint )" "} else {")"
if [[ -z "$vm_native_body" ]]; then
	fail "could not extract native VM_Call branch"
fi

assert_contains "$vm_native_body" "int32_t args[MAX_VMMAIN_CALL_ARGS-1];" "native VM arg storage"
assert_contains "$vm_native_body" "Com_Memset( args, 0, sizeof( args ) );" "native VM arg zero-fill"
assert_contains "$vm_native_body" "for ( i = 0; i < nargs; i++ )" "native VM vararg copy loop"
assert_contains "$vm_native_body" "args[i] = va_arg( ap, int32_t );" "native VM compact arg copy"
assert_contains "$vm_native_body" "r = vm->entryPoint( callnum, args[0], args[1], args[2] );" "native VM stable three-arg dispatch"
assert_order "$vm_native_body" "Com_Memset( args, 0, sizeof( args ) );" "va_start( ap, callnum );" "zero-fill before reading varargs"
assert_order "$vm_native_body" "va_end( ap );" "r = vm->entryPoint( callnum, args[0], args[1], args[2] );" "finish varargs before native dispatch"
assert_not_contains "$vm_native_body" "args[i+1] = va_arg" "native VM branch must not leave args[0] as callnum"

fs_startup_body="$(extract_function "$FILES_SOURCE" "FS_Startup")"
if [[ -z "$fs_startup_body" ]]; then
	fail "could not extract FS_Startup"
fi
assert_contains "$fs_startup_body" "com_nativeLibraryExtractPk3 = Cvar_Get( \"com_nativeLibraryExtractPk3\", \"1\", CVAR_ARCHIVE );" "pk3 extraction cvar default"
assert_contains "$fs_startup_body" "Cvar_SetDescription( com_nativeLibraryExtractPk3" "pk3 extraction cvar description"
assert_contains "$fs_startup_body" "Com_Printf( \"com_nativeLibraryExtractPk3: extracting embedded native libs from pk3 is enabled.\\n\" );" "pk3 extraction startup log"

pk3_body="$(extract_function "$FILES_SOURCE" "FS_TryLoadLibraryFromPk3Cache")"
if [[ -z "$pk3_body" ]]; then
	fail "could not extract FS_TryLoadLibraryFromPk3Cache"
fi
assert_contains "$pk3_body" "if ( !com_nativeLibraryExtractPk3 || !com_nativeLibraryExtractPk3->integer )" "pk3 cache cvar gate"
assert_contains "$pk3_body" "if ( !name || !name[0] )" "pk3 cache empty-name guard"
assert_contains "$pk3_body" "slash = strrchr( name, '/' );" "pk3 cache basename extraction"
assert_contains "$pk3_body" "base = slash + 1;" "pk3 cache ignores path components for cache qpath"
assert_contains "$pk3_body" "Q_stricmp( base + strlen( base ) - 3, \".so\" ) != 0" "pk3 cache native extension filter"
assert_contains "$pk3_body" "Com_sprintf( cacheQpath, sizeof( cacheQpath ), \"%s%s\", FS_NATIVE_LIB_CACHE_PREFIX, base );" "pk3 cache basename-only qpath"
assert_contains "$pk3_body" "len = FS_ReadFile( name, &fileBuf );" "pk3 cache direct read attempt"
assert_contains "$pk3_body" "Com_sprintf( alt, sizeof( alt ), \"vm/%s\", name );" "pk3 cache vm fallback"
assert_contains "$pk3_body" "Com_sprintf( alt, sizeof( alt ), \"modules/%s\", name );" "pk3 cache modules fallback"
assert_order "$pk3_body" "if ( slash )" "Com_sprintf( alt, sizeof( alt ), \"vm/%s\", name );" "explicit path skips implicit fallback probes"
assert_order "$pk3_body" "crcPak = crc32_buffer" "Q_strncpyz( osCachePath" "pk3 cache hashes source before cache reuse"
assert_order "$pk3_body" "crcDisk = crc32_buffer" "if ( crcDisk == crcPak )" "pk3 cache compares disk CRC"
assert_order "$pk3_body" "if ( crcDisk == crcPak )" "h = FS_TryLoadLibraryPath( osCachePath );" "pk3 cache loads only after CRC match"
assert_contains "$pk3_body" "FILE *out = Sys_FOpen( osCachePath, \"wb\" );" "pk3 cache direct OS write"
assert_contains "$pk3_body" "fwrite( fileBuf, 1, (size_t)len, out )" "pk3 cache writes full payload"
assert_not_contains "$pk3_body" "FS_WriteFile" "pk3 cache must bypass FS write filters for native libraries"
assert_contains "$pk3_body" "FS_LoadLibrary: using pk3 native cache" "pk3 cache reuse log"
assert_contains "$pk3_body" "FS_LoadLibrary: extracted pk3 native lib" "pk3 cache extraction log"

fs_load_body="$(extract_function "$FILES_SOURCE" "FS_LoadLibrary")"
if [[ -z "$fs_load_body" ]]; then
	fail "could not extract FS_LoadLibrary"
fi
assert_contains "$fs_load_body" "libHandle = FS_TryLoadLibraryFromPk3Cache( name );" "FS_LoadLibrary pk3 cache probe"
assert_order "$fs_load_body" "libHandle = FS_TryLoadLibraryFromPk3Cache( name );" "while ( !libHandle && sp )" "FS_LoadLibrary tries pk3 cache before loose filesystem paths"

echo "PASS: test_native_module_regressions"
