#!/usr/bin/env bash
# Regression test for scripts/compile_engine.sh LTO option wiring.
# Verifies that argument parsing toggles -DENABLE_LTO between ON/OFF deterministically.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
SCRIPT_UNDER_TEST="${1:-$PROJECT_ROOT/scripts/compile_engine.sh}"

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

assert_contains() {
	local haystack="$1"
	local needle="$2"
	local context="$3"
	if [[ "$haystack" != *"$needle"* ]]; then
		fail "$context: expected '$needle' in '$haystack'"
	fi
}

assert_not_contains() {
	local haystack="$1"
	local needle="$2"
	local context="$3"
	if [[ "$haystack" == *"$needle"* ]]; then
		fail "$context: unexpected '$needle' in '$haystack'"
	fi
}

make_fixture() {
	local case_dir="$1"
	mkdir -p "$case_dir/scripts"
	cp "$SCRIPT_UNDER_TEST" "$case_dir/scripts/compile_engine.sh"
	chmod +x "$case_dir/scripts/compile_engine.sh"
	cat > "$case_dir/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.20)
project(mock_engine LANGUAGES C)
EOF
}

make_cmake_stub() {
	local bin_dir="$1"
	mkdir -p "$bin_dir"
	cat > "$bin_dir/cmake" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
log_file="${MOCK_CMAKE_LOG:?MOCK_CMAKE_LOG is required}"
printf '%s\n' "$*" >> "$log_file"
exit 0
EOF
	chmod +x "$bin_dir/cmake"
}

extract_configure_call() {
	local log_file="$1"
	local line
	while IFS= read -r line; do
		if [[ "$line" == *"-S "* && "$line" == *"-B "* ]]; then
			echo "$line"
			return 0
		fi
	done < "$log_file"
	fail "did not capture a cmake configure call"
}

run_case() {
	local case_name="$1"
	local args="$2"
	local case_dir="$TMP_ROOT/$case_name"
	local bin_dir="$case_dir/mock-bin"
	local log_file="$case_dir/cmake.log"
	local output_file="$case_dir/output.log"

	make_fixture "$case_dir"
	make_cmake_stub "$bin_dir"

	if ! env -i \
		PATH="$bin_dir:/usr/bin:/bin" \
		HOME="$TMP_ROOT/home" \
		MOCK_CMAKE_LOG="$log_file" \
		bash "$case_dir/scripts/compile_engine.sh" $args >"$output_file" 2>&1; then
		fail "$case_name: compile_engine.sh exited non-zero"
	fi

	printf '%s\n' "$log_file" "$output_file"
}

if [ ! -f "$SCRIPT_UNDER_TEST" ]; then
	fail "compile_engine script not found: $SCRIPT_UNDER_TEST"
fi

TMP_ROOT="$(mktemp -d)"
trap 'rm -rf "$TMP_ROOT"' EXIT
mkdir -p "$TMP_ROOT/home"

# Case 1: no lto arg should pass ENABLE_LTO=OFF and avoid LTO status log.
mapfile -t case1_files < <(run_case "case-no-lto" "opengl")
case1_cfg="$(extract_configure_call "${case1_files[0]}")"
case1_output="$(<"${case1_files[1]}")"
assert_contains "$case1_cfg" "-DENABLE_LTO=OFF" "case-no-lto configure flag"
assert_not_contains "$case1_output" "CMake: ENABLE_LTO=ON" "case-no-lto output"

# Case 2: lto arg should pass ENABLE_LTO=ON and print explicit startup log.
mapfile -t case2_files < <(run_case "case-with-lto" "opengl lto")
case2_cfg="$(extract_configure_call "${case2_files[0]}")"
case2_output="$(<"${case2_files[1]}")"
assert_contains "$case2_cfg" "-DENABLE_LTO=ON" "case-with-lto configure flag"
assert_contains "$case2_output" "CMake: ENABLE_LTO=ON (IPO/LTO for Release/RelWithDebInfo on GCC/Clang; expect longer links)" "case-with-lto output"

echo "PASS: test_compile_engine_lto"
