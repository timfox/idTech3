#!/usr/bin/env bash
# Regression tests for scripts/run_vulkan.sh launcher selection.
# Usage: test_run_vulkan.sh [path/to/run_vulkan.sh]
#        Default second arg: <repo>/scripts/run_vulkan.sh (from tests/.. layout).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
if [ -n "${1:-}" ]; then
	SCRIPT_UNDER_TEST="$1"
else
	SCRIPT_UNDER_TEST="$PROJECT_ROOT/scripts/run_vulkan.sh"
fi
if [ ! -f "$SCRIPT_UNDER_TEST" ]; then
	echo "Error: run_vulkan script not found: $SCRIPT_UNDER_TEST" >&2
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
		fail "$context: expected '$needle' in output: $haystack"
	fi
}

assert_not_contains() {
	local haystack="$1"
	local needle="$2"
	local context="$3"
	if [[ "$haystack" == *"$needle"* ]]; then
		fail "$context: unexpected '$needle' in output: $haystack"
	fi
}

make_engine_stub() {
	local path="$1"
	local marker="$2"
	cat > "$path" <<EOF
#!/usr/bin/env bash
echo "ENGINE_MARKER:${marker}"
for arg in "\$@"; do
	echo "ARG:\$arg"
done
EOF
	chmod +x "$path"
}

make_uname_stub() {
	local bin_dir="$1"
	local machine="$2"
	mkdir -p "$bin_dir"
	cat > "$bin_dir/uname" <<EOF
#!/usr/bin/env bash
if [ "\${1:-}" = "-m" ]; then
	echo "${machine}"
	exit 0
fi
exec /usr/bin/uname "\$@"
EOF
	chmod +x "$bin_dir/uname"
}

run_launcher() {
	local launcher="$1"
	local machine="$2"
	shift 2
	local bin_dir
	bin_dir="$(mktemp -d "$TMP_ROOT/uname-XXXXXX")"
	make_uname_stub "$bin_dir" "$machine"
	env -i \
		PATH="$bin_dir:/usr/bin:/bin" \
		HOME="$TMP_ROOT/home" \
		bash "$launcher" "$@"
}

TMP_ROOT="$(mktemp -d)"
trap 'rm -rf "$TMP_ROOT"' EXIT
mkdir -p "$TMP_ROOT/home"

# Case 1: when run from scripts/, use sibling ../release/idtech3 (not a binary in scripts/).
CASE1="$TMP_ROOT/case1"
mkdir -p "$CASE1/scripts" "$CASE1/release"
cp "$SCRIPT_UNDER_TEST" "$CASE1/scripts/run_vulkan.sh"
chmod +x "$CASE1/scripts/run_vulkan.sh"
make_engine_stub "$CASE1/release/idtech3" "case1-release"
make_engine_stub "$CASE1/scripts/idtech3" "case1-scripts"

out_case1="$(run_launcher "$CASE1/scripts/run_vulkan.sh" "x86_64" +set cl_renderer vulkan)"
assert_contains "$out_case1" "ENGINE_MARKER:case1-release" "case1 release preference"
assert_not_contains "$out_case1" "ENGINE_MARKER:case1-scripts" "case1 release preference"
assert_contains "$out_case1" "ARG:+set" "case1 arg forwarding"
assert_contains "$out_case1" "ARG:cl_renderer" "case1 arg forwarding"
assert_contains "$out_case1" "ARG:vulkan" "case1 arg forwarding"

# Case 2: on aarch64, prefer idtech3.aarch64 when present and executable.
CASE2="$TMP_ROOT/case2"
mkdir -p "$CASE2/scripts" "$CASE2/release"
cp "$SCRIPT_UNDER_TEST" "$CASE2/scripts/run_vulkan.sh"
chmod +x "$CASE2/scripts/run_vulkan.sh"
make_engine_stub "$CASE2/release/idtech3.aarch64" "case2-aarch64"
make_engine_stub "$CASE2/release/idtech3" "case2-default"

out_case2="$(run_launcher "$CASE2/scripts/run_vulkan.sh" "aarch64" +set r_mode -1)"
assert_contains "$out_case2" "ENGINE_MARKER:case2-aarch64" "case2 aarch64 binary selection"
assert_not_contains "$out_case2" "ENGINE_MARKER:case2-default" "case2 aarch64 binary selection"

# Case 3: on aarch64, fall back to idtech3 when idtech3.aarch64 exists but is not executable.
CASE3="$TMP_ROOT/case3"
mkdir -p "$CASE3/scripts" "$CASE3/release"
cp "$SCRIPT_UNDER_TEST" "$CASE3/scripts/run_vulkan.sh"
chmod +x "$CASE3/scripts/run_vulkan.sh"
make_engine_stub "$CASE3/release/idtech3.aarch64" "case3-aarch64-unused"
chmod -x "$CASE3/release/idtech3.aarch64"
make_engine_stub "$CASE3/release/idtech3" "case3-fallback"

out_case3="$(run_launcher "$CASE3/scripts/run_vulkan.sh" "aarch64" +set developer 1)"
assert_contains "$out_case3" "ENGINE_MARKER:case3-fallback" "case3 non-exec aarch64 fallback"
assert_not_contains "$out_case3" "ENGINE_MARKER:case3-aarch64-unused" "case3 non-exec aarch64 fallback"
assert_contains "$out_case3" "ARG:+set" "case3 arg forwarding"
assert_contains "$out_case3" "ARG:developer" "case3 arg forwarding"
assert_contains "$out_case3" "ARG:1" "case3 arg forwarding"

echo "PASS: test_run_vulkan"
