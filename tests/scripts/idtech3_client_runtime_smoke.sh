#!/usr/bin/env bash
# Shared optional client runtime probe. Headless/no-Vulkan systems skip unless
# IDTECH3_RUNTIME_REQUIRED=1 is set by a renderer-capable runner.
set -euo pipefail

idtech3_client_root() {
	cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd
}

idtech3_client_binary() {
	local root="$1"
	local release_dir="${RELEASE_DIR:-$root/release}"
	local build_dir="${BUILD_DIR:-$root/build-vk-Release}"
	local candidate

	for candidate in \
		"$build_dir/idtech3" \
		"$build_dir/idtech3.x86_64" \
		"$release_dir/idtech3" \
		"$release_dir/idtech3.x86_64" \
		"$release_dir/idtech3.exe"; do
		if [[ -x "$candidate" || -f "$candidate" ]]; then
			printf '%s\n' "$candidate"
			return 0
		fi
	done
	return 1
}

idtech3_client_pack() {
	local root="$1"
	local base="$root/docs/renderer_validation/devdata/rtest_base"

	[[ -f "$base/default.cfg" ]] || return 1
	[[ -f "$base/maps/rtest_parity.bsp" ]] || return 1
	printf '%s\n' "$base"
	return 0
}

idtech3_client_is_skip_log() {
	local output="$1"
	echo "$output" | grep -Eiq 'could not load Vulkan subsystem|Vulkan support.*not available|SDL_CreateWindow.*failed|no display|DISPLAY.*not set|VK_ERROR_INITIALIZATION_FAILED'
}

idtech3_client_run_optional() {
	local label="$1"
	shift
	local root
	local client
	local game_base
	local install_root
	local game_dir
	local output
	local rc=0
	local timeout_sec="${IDTECH3_CLIENT_TIMEOUT:-30}"
	local required="${IDTECH3_RUNTIME_REQUIRED:-0}"

	root="$(idtech3_client_root)"
	if ! client="$(idtech3_client_binary "$root")"; then
		if [[ "$required" == "1" ]]; then
			echo "FAIL: $label client runtime required but idtech3 client is not built" >&2
			return 1
		fi
		echo "SKIP: $label client runtime (idtech3 client not built)"
		return 77
	fi
	if ! game_base="$(idtech3_client_pack "$root")"; then
		if [[ "$required" == "1" ]]; then
			echo "FAIL: $label client runtime required but minimal pack is missing" >&2
			return 1
		fi
		echo "SKIP: $label client runtime (minimal pack missing)"
		return 77
	fi

	install_root="$(cd "$(dirname "$game_base")" && pwd)"
	game_dir="$(basename "$game_base")"
	output="$(timeout "$timeout_sec" env \
		SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-dummy}" \
		SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-dummy}" \
		"$client" \
		+set fs_basepath "$install_root" \
		+set fs_game "$game_dir" \
		+set vm_game 2 \
		+set net_enabled 0 \
		+set ttycon 0 \
		"$@" 2>&1)" || rc=$?

	if [[ "$rc" -ne 0 ]]; then
		if [[ "$required" != "1" ]] && idtech3_client_is_skip_log "$output"; then
			echo "SKIP: $label client runtime (headless/no Vulkan)"
			return 77
		fi
		echo "$output" >&2
		echo "FAIL: $label client runtime exited with $rc" >&2
		return "$rc"
	fi

	if echo "$output" | grep -Eiq 'Sys_Error|Server fatal crashed|SIGSEGV|segfault|core dump|\babort\b'; then
		echo "$output" >&2
		echo "FAIL: $label client runtime reported fatal error" >&2
		return 1
	fi

	printf '%s\n' "$output"
	return 0
}
