#!/usr/bin/env bash
# Deterministic coverage for renderer_regression_maps.sh without requiring game data.
set -euo pipefail

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

assert_contains() {
	local haystack="$1"
	local needle="$2"
	if [[ "$haystack" != *"$needle"* ]]; then
		fail "missing expected text: $needle"
	fi
}

assert_file_contains() {
	local file="$1"
	local needle="$2"
	if ! grep -Fq -- "$needle" "$file"; then
		echo "--- $file ---" >&2
		sed -n '1,120p' "$file" >&2
		fail "missing expected text in $file: $needle"
	fi
}

assert_exit_code() {
	local expected="$1"
	local actual="$2"
	if [[ "$actual" -ne "$expected" ]]; then
		fail "expected exit code $expected, got $actual"
	fi
}

if [[ $# -gt 1 ]]; then
	fail "usage: $0 [path/to/renderer_regression_maps.sh]"
fi

SCRIPT_UNDER_TEST="${1:-}"
if [[ -z "$SCRIPT_UNDER_TEST" ]]; then
	SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
	SCRIPT_UNDER_TEST="$(cd "$SCRIPT_DIR/../.." && pwd)/scripts/renderer_regression_maps.sh"
fi

[[ -f "$SCRIPT_UNDER_TEST" ]] || fail "script under test not found: $SCRIPT_UNDER_TEST"

FIXTURE="$(mktemp -d)"
cleanup() { rm -rf "$FIXTURE"; }
trap cleanup EXIT

mkdir -p "$FIXTURE/scripts" "$FIXTURE/release" "$FIXTURE/game_root/base"
touch "$FIXTURE/CMakeLists.txt"
cp "$SCRIPT_UNDER_TEST" "$FIXTURE/scripts/renderer_regression_maps.sh"
chmod +x "$FIXTURE/scripts/renderer_regression_maps.sh"

SERVER_LOG="$FIXTURE/server_args.log"
cat > "$FIXTURE/release/idtech3_server" <<'STUB'
#!/usr/bin/env bash
set -euo pipefail

log="${STUB_SERVER_LOG:?}"
{
	printf 'CALL'
	for arg in "$@"; do
		printf '\t%s' "$arg"
	done
	printf '\n'
} >> "$log"

map=""
previous=""
for arg in "$@"; do
	if [[ "$previous" == "+map" ]]; then
		map="$arg"
		break
	fi
	previous="$arg"
done

if [[ "${STUB_SERVER_MODE:-pass}" == "error-on-pbr" && "$map" == "rtest_pbr" ]]; then
	echo "ERROR: simulated renderer map load failure for $map"
	exit 0
fi

echo "Server: $map"
STUB
chmod +x "$FIXTURE/release/idtech3_server"

out="$FIXTURE/pass.out"
GAME_BASE="$FIXTURE/game_root/base" \
	RELEASE_DIR="$FIXTURE/release" \
	STUB_SERVER_LOG="$SERVER_LOG" \
	"$FIXTURE/scripts/renderer_regression_maps.sh" > "$out" 2>&1

pass_output="$(<"$out")"
assert_contains "$pass_output" "fs_basepath -> $FIXTURE/game_root (game dir: base)"
assert_contains "$pass_output" "=== Summary === Passed: 6  Failed: 0"
assert_contains "$pass_output" "MAP LOAD SANITY PASSED"

call_count="$(grep -c '^CALL' "$SERVER_LOG")"
[[ "$call_count" -eq 6 ]] || fail "expected six map server calls, got $call_count"

for map in rtest_tangent rtest_pbr rtest_emissive rtest_volumetric rtest_postfx rtest_parity; do
	assert_file_contains "$SERVER_LOG" $'+set\tdedicated\t1'
	assert_file_contains "$SERVER_LOG" $'+set\tfs_basepath\t'"$FIXTURE/game_root"
	assert_file_contains "$SERVER_LOG" $'+set\tfs_game\tbase'
	assert_file_contains "$SERVER_LOG" $'+set\tvm_game\t2'
	assert_file_contains "$SERVER_LOG" $'+set\tbot_enable\t0'
	assert_file_contains "$SERVER_LOG" $'+set\tcom_hunkMegs\t128'
	assert_file_contains "$SERVER_LOG" $'+map\t'"$map"
done

error_out="$FIXTURE/error.out"
set +e
GAME_BASE="$FIXTURE/game_root/base" \
	RELEASE_DIR="$FIXTURE/release" \
	STUB_SERVER_LOG="$SERVER_LOG" \
	STUB_SERVER_MODE=error-on-pbr \
	"$FIXTURE/scripts/renderer_regression_maps.sh" > "$error_out" 2>&1
status=$?
set -e
assert_exit_code 1 "$status"
error_output="$(<"$error_out")"
assert_contains "$error_output" "simulated renderer map load failure for rtest_pbr"
assert_contains "$error_output" "map rtest_pbr: error in server log"
assert_contains "$error_output" "MAP LOAD SANITY FAILED"

missing_base_out="$FIXTURE/missing-base.out"
set +e
RELEASE_DIR="$FIXTURE/release" \
	"$FIXTURE/scripts/renderer_regression_maps.sh" > "$missing_base_out" 2>&1
status=$?
set -e
assert_exit_code 2 "$status"
assert_contains "$(<"$missing_base_out")" "GAME_BASE = game **base** directory"

echo "OK: renderer_regression_maps.sh argument mapping and failure detection"
