#!/usr/bin/env bash
# Regression tests for scripts/compile_engine.sh ENABLE_LTO flag plumbing.
# Usage: test_compile_engine_lto.sh [path/to/compile_engine.sh]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
if [ -n "${1:-}" ]; then
	SCRIPT_UNDER_TEST="$1"
else
	SCRIPT_UNDER_TEST="$PROJECT_ROOT/scripts/compile_engine.sh"
fi
if [ ! -f "$SCRIPT_UNDER_TEST" ]; then
	echo "Error: compile_engine script not found: $SCRIPT_UNDER_TEST" >&2
	exit 2
fi

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

assert_contains() {
	local haystack="$1"
	local needle="$2"
	local context="$3"
	if [[ "$haystack" != *"$needle"* ]]; then
		fail "$context: expected '$needle' in: $haystack"
	fi
}

make_cmake_stub() {
	local bin_dir="$1"
	mkdir -p "$bin_dir"
	cat > "$bin_dir/cmake" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
printf 'CMAKE_CALL:%s\n' "$*" >> "${CMAKE_LOG:?}"
exit 0
EOF
	chmod +x "$bin_dir/cmake"
}

make_fake_project() {
	local root="$1"
	mkdir -p "$root/scripts"
	touch "$root/CMakeLists.txt"
	cp "$SCRIPT_UNDER_TEST" "$root/scripts/compile_engine.sh"
	chmod +x "$root/scripts/compile_engine.sh"
}

run_compile_script() {
	local launcher="$1"
	local log="$2"
	shift 2
	local bin_dir
	bin_dir="$(mktemp -d "$TMP_ROOT/bin-XXXXXX")"
	make_cmake_stub "$bin_dir"
	env -i \
		PATH="$bin_dir:/usr/bin:/bin" \
		HOME="$TMP_ROOT/home" \
		CMAKE_LOG="$log" \
		bash "$launcher" "$@"
}

TMP_ROOT="$(mktemp -d)"
trap 'rm -rf "$TMP_ROOT"' EXIT
mkdir -p "$TMP_ROOT/home"

# Case 1: lto argument must pass ENABLE_LTO=ON into CMake configure flags.
CASE1="$TMP_ROOT/case1"
make_fake_project "$CASE1"
LOG1="$TMP_ROOT/cmake_case1.log"
run_compile_script "$CASE1/scripts/compile_engine.sh" "$LOG1" opengl skipshaders lto
log_case1="$(<"$LOG1")"
assert_contains "$log_case1" "-DENABLE_LTO=ON" "case1 lto enabled"
assert_contains "$log_case1" "-DRENDERER_DEFAULT=opengl" "case1 opengl renderer flag"

# Case 2: without lto, compile script must keep ENABLE_LTO=OFF.
CASE2="$TMP_ROOT/case2"
make_fake_project "$CASE2"
LOG2="$TMP_ROOT/cmake_case2.log"
run_compile_script "$CASE2/scripts/compile_engine.sh" "$LOG2" opengl skipshaders
log_case2="$(<"$LOG2")"
assert_contains "$log_case2" "-DENABLE_LTO=OFF" "case2 lto disabled default"

echo "PASS: test_compile_engine_lto"
