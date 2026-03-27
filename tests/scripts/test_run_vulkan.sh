#!/usr/bin/env bash
# Unit test for scripts/run_vulkan.sh launcher behavior.
set -euo pipefail

SCRIPT_UNDER_TEST="${1:-}"
if [ -z "$SCRIPT_UNDER_TEST" ]; then
	SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
	SCRIPT_UNDER_TEST="$(cd "$SCRIPT_DIR/../.." && pwd)/scripts/run_vulkan.sh"
fi

if [ ! -f "$SCRIPT_UNDER_TEST" ]; then
	echo "FAIL: run_vulkan.sh not found at '$SCRIPT_UNDER_TEST'" >&2
	exit 1
fi

TMP_ROOT="$(mktemp -d)"
cleanup() {
	rm -rf "$TMP_ROOT"
}
trap cleanup EXIT

assert_contains() {
	local haystack="$1"
	local needle="$2"
	local message="$3"
	if [[ "$haystack" != *"$needle"* ]]; then
		echo "FAIL: $message" >&2
		echo "Expected to find: $needle" >&2
		echo "Actual output:" >&2
		printf '%s\n' "$haystack" >&2
		exit 1
	fi
}

make_fake_uname() {
	local bin_dir="$1"
	local arch="$2"

	mkdir -p "$bin_dir"
	cat > "$bin_dir/uname" <<EOF
#!/usr/bin/env bash
echo "$arch"
EOF
	chmod +x "$bin_dir/uname"
}

make_fake_engine() {
	local path="$1"

	mkdir -p "$(dirname "$path")"
	cat > "$path" <<'EOF'
#!/usr/bin/env bash
echo "ENGINE:$0"
echo "ARGS:$*"
echo "LDLP:${LD_LIBRARY_PATH:-}"
EOF
	chmod +x "$path"
}

copy_script() {
	local target_path="$1"
	mkdir -p "$(dirname "$target_path")"
	cp "$SCRIPT_UNDER_TEST" "$target_path"
	chmod +x "$target_path"
}

test_repo_layout_uses_release_dir() {
	local root="$TMP_ROOT/repo_release"
	local script_path="$root/scripts/run_vulkan.sh"
	local mock_bin="$root/mockbin"
	local home_dir="$root/home"
	local output

	copy_script "$script_path"
	make_fake_engine "$root/release/idtech3"
	make_fake_uname "$mock_bin" "x86_64"
	mkdir -p "$home_dir"

	output="$(PATH="$mock_bin:$PATH" HOME="$home_dir" "$script_path" +set cl_renderer vulkan)"

	assert_contains "$output" "ENGINE:$root/release/idtech3" "script should launch release/idtech3 when run from scripts/"
	assert_contains "$output" "ARGS:+set cl_renderer vulkan" "script should preserve arguments"
}

test_arch_specific_binary_is_preferred() {
	local root="$TMP_ROOT/arch_specific"
	local script_path="$root/scripts/run_vulkan.sh"
	local mock_bin="$root/mockbin"
	local home_dir="$root/home"
	local output

	copy_script "$script_path"
	make_fake_engine "$root/release/idtech3"
	make_fake_engine "$root/release/idtech3.aarch64"
	make_fake_uname "$mock_bin" "aarch64"
	mkdir -p "$home_dir"

	output="$(PATH="$mock_bin:$PATH" HOME="$home_dir" "$script_path")"

	assert_contains "$output" "ENGINE:$root/release/idtech3.aarch64" "aarch64 should prefer idtech3.aarch64 when executable exists"
}

test_arch_specific_fallback_to_generic_binary() {
	local root="$TMP_ROOT/arch_fallback"
	local script_path="$root/scripts/run_vulkan.sh"
	local mock_bin="$root/mockbin"
	local home_dir="$root/home"
	local output

	copy_script "$script_path"
	make_fake_engine "$root/release/idtech3"
	mkdir -p "$root/release"
	printf '#!/usr/bin/env bash\nexit 0\n' > "$root/release/idtech3.aarch64"
	chmod 0644 "$root/release/idtech3.aarch64"
	make_fake_uname "$mock_bin" "aarch64"
	mkdir -p "$home_dir"

	output="$(PATH="$mock_bin:$PATH" HOME="$home_dir" "$script_path")"

	assert_contains "$output" "ENGINE:$root/release/idtech3" "aarch64 should fall back to idtech3 when idtech3.aarch64 is not executable"
}

test_non_repo_layout_falls_back_to_script_dir() {
	local root="$TMP_ROOT/script_dir_fallback"
	local script_path="$root/bin/run_vulkan.sh"
	local mock_bin="$root/mockbin"
	local home_dir="$root/home"
	local output

	copy_script "$script_path"
	make_fake_engine "$root/bin/idtech3"
	make_fake_uname "$mock_bin" "x86_64"
	mkdir -p "$home_dir"

	output="$(PATH="$mock_bin:$PATH" HOME="$home_dir" "$script_path")"

	assert_contains "$output" "ENGINE:$root/bin/idtech3" "script should fall back to its own directory if ../release is unavailable"
}

test_ld_library_path_prepend_behavior() {
	local root="$TMP_ROOT/ld_library_path"
	local script_path="$root/scripts/run_vulkan.sh"
	local mock_bin="$root/mockbin"
	local home_dir="$root/home"
	local output
	local expected_ldlp

	copy_script "$script_path"
	make_fake_engine "$root/release/idtech3"
	make_fake_uname "$mock_bin" "x86_64"
	mkdir -p "$home_dir/sdl2-vulkan-install/lib"
	touch "$home_dir/sdl2-vulkan-install/lib/libSDL2.so"

	output="$(PATH="$mock_bin:$PATH" HOME="$home_dir" LD_LIBRARY_PATH="/opt/custom" "$script_path")"

	if [ -f "/usr/local/lib/libSDL2.so" ] || [ -f "/usr/local/lib/aarch64-linux-gnu/libSDL2.so" ]; then
		expected_ldlp="/usr/local/lib:/opt/custom"
	else
		expected_ldlp="$home_dir/sdl2-vulkan-install/lib:/opt/custom"
	fi

	assert_contains "$output" "LDLP:$expected_ldlp" "script should prepend selected SDL path and preserve LD_LIBRARY_PATH"
}

test_repo_layout_uses_release_dir
test_arch_specific_binary_is_preferred
test_arch_specific_fallback_to_generic_binary
test_non_repo_layout_falls_back_to_script_dir
test_ld_library_path_prepend_behavior

echo "PASS: unit_run_vulkan_script"
