#!/usr/bin/env bash
# Shared headless smoke helpers for minimal-content end-to-end tests.
set -euo pipefail

idtech3_minimal_root() {
	cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd
}

idtech3_minimal_server() {
	local root="$1"
	local release_dir="${RELEASE_DIR:-$root/release}"
	local build_dir="${BUILD_DIR:-$root/build-vk-Release}"
	local candidate

	for candidate in \
		"$build_dir/idtech3_server" \
		"$build_dir/idtech3_server.x86_64" \
		"$build_dir/idtech3_server.x64" \
		"$release_dir/idtech3_server" \
		"$release_dir/idtech3_server.x86_64" \
		"$release_dir/idtech3_server.x64" \
		"$release_dir/idtech3_server.exe"; do
		if [[ -x "$candidate" || -f "$candidate" ]]; then
			printf '%s\n' "$candidate"
			return 0
		fi
	done

	return 1
}

idtech3_minimal_require_pack() {
	local root="$1"
	local base="$root/docs/renderer_validation/devdata/rtest_base"

	[[ -f "$base/default.cfg" ]] || return 1
	[[ -f "$base/vm/qagame.qvm" ]] || return 1
	[[ -f "$base/maps/rtest_parity.bsp" ]] || return 1
	printf '%s\n' "$base"
	return 0
}

idtech3_minimal_run_server() {
	local root="$1"
	shift
	local server
	local game_base
	local install_root
	local game_dir
	local timeout_sec="${IDTECH3_MINIMAL_TIMEOUT:-45}"

	if ! server="$(idtech3_minimal_server "$root")"; then
		echo "[minimal_content_smoke] skip: idtech3_server not built (set BUILD_DIR or RELEASE_DIR)" >&2
		return 77
	fi
	if ! game_base="$(idtech3_minimal_require_pack "$root")"; then
		echo "[minimal_content_smoke] missing docs/renderer_validation/devdata/rtest_base minimal pack" >&2
		return 2
	fi

	install_root="$(cd "$(dirname "$game_base")" && pwd)"
	game_dir="$(basename "$game_base")"

	timeout "$timeout_sec" "$server" \
		+set dedicated 1 \
		+set fs_basepath "$install_root" \
		+set fs_game "$game_dir" \
		+set vm_game 2 \
		+set bot_enable 0 \
		+set net_enabled 0 \
		+set com_hunkMegs 128 \
		"$@" 2>&1 || true
}

idtech3_minimal_fail_pattern() {
	printf '%s\n' 'ERROR:|Server fatal crashed|couldn'\''t load|could not load|CM_LoadMap:|SIGSEGV|segfault|core dump|\babort\b'
}

idtech3_minimal_assert_clean_log() {
	local output="$1"
	local fail_pattern
	fail_pattern="$(idtech3_minimal_fail_pattern)"

	if echo "$output" | grep -Eiv "Couldn't load symbol file" | grep -Eiq "$fail_pattern"; then
		echo "$output" | grep -Eiv "Couldn't load symbol file" | grep -Ei "$fail_pattern" | head -20 >&2
		return 1
	fi
	return 0
}

idtech3_minimal_map_smoke() {
	local root="$1"
	local output
	local rc=0

	output="$(idtech3_minimal_run_server "$root" +map rtest_parity +quit)" || rc=$?
	if [[ "$rc" -eq 77 ]]; then
		return 77
	fi
	if [[ "$rc" -ne 0 ]]; then
		return "$rc"
	fi
	idtech3_minimal_assert_clean_log "$output"
	echo "$output" | grep -q 'Server: rtest_parity'
	echo "$output" | grep -q 'InitGame:'
	echo "$output" | grep -q 'qagame loaded'
	echo "[minimal_content_smoke] map+VM: ok"
}

idtech3_minimal_physics_smoke() {
	local root="$1"
	local output
	local rc=0

	output="$(idtech3_minimal_run_server "$root" \
		+set phys_enabled 1 \
		+set sv_physSpawn 1 \
		+map rtest_parity \
		+phys_status \
		+phys_spawn_box 0 0 96 12 \
		+phys_status \
		+quit)" || rc=$?
	if [[ "$rc" -eq 77 ]]; then
		return 77
	fi
	if [[ "$rc" -ne 0 ]]; then
		return "$rc"
	fi
	idtech3_minimal_assert_clean_log "$output"
	echo "$output" | grep -q 'Server: rtest_parity'
	echo "$output" | grep -q 'PhysMiddleware: gameplay physics layer ready'
	echo "$output" | grep -q 'Physics middleware status:'
	echo "$output" | grep -q 'phys_spawn_box: body='
	echo "$output" | grep -q 'demo props:[[:space:]]*1'
	echo "[minimal_content_smoke] physics commands: ok"
}

idtech3_minimal_app_crdt_smoke() {
	local root="$1"
	local output
	local rc=0

	output="$(idtech3_minimal_run_server "$root" \
		+set com_app_crdt 1 \
		+map rtest_parity \
		+app_crdt_status \
		+app_crdt_publish 1.2.3 \
		+app_crdt_emit smoke_payload \
		+quit)" || rc=$?
	if [[ "$rc" -eq 77 ]]; then
		return 77
	fi
	if [[ "$rc" -ne 0 ]]; then
		return "$rc"
	fi
	idtech3_minimal_assert_clean_log "$output"
	echo "$output" | grep -q 'Server: rtest_parity'
	echo "$output" | grep -q '\[AppCRDT\] enabled=1'
	echo "$output" | grep -q '\[AppCRDT\] published 1.2.3 to all clients'
	echo "$output" | grep -q '\[AppCRDT\] emitted event major=1'
	echo "[minimal_content_smoke] App CRDT server commands: ok"
}

idtech3_minimal_oscar_smoke() {
	local root="$1"
	local output
	local rc=0

	output="$(idtech3_minimal_run_server "$root" \
		+set oscar_enable 1 \
		+set oscar_mode direct \
		+map rtest_parity \
		+oscar_status \
		+oscar_buddies \
		+oscar_disconnect \
		+quit)" || rc=$?
	if [[ "$rc" -eq 77 ]]; then
		return 77
	fi
	if [[ "$rc" -ne 0 ]]; then
		return "$rc"
	fi
	idtech3_minimal_assert_clean_log "$output"
	echo "$output" | grep -q 'OSCAR direct client: enabled'
	echo "$output" | grep -q 'OSCAR available: yes'
	echo "$output" | grep -q 'OSCAR buddy roster'
	echo "[minimal_content_smoke] OSCAR server commands: ok"
}

idtech3_minimal_run_case() {
	local case_name="${1:-all}"
	local root
	local status=0

	root="$(idtech3_minimal_root)"

	case "$case_name" in
		map)
			idtech3_minimal_map_smoke "$root" || status=$?
			;;
		physics)
			idtech3_minimal_physics_smoke "$root" || status=$?
			;;
		app_crdt)
			idtech3_minimal_app_crdt_smoke "$root" || status=$?
			;;
		oscar)
			idtech3_minimal_oscar_smoke "$root" || status=$?
			;;
		all)
			idtech3_minimal_map_smoke "$root" || status=$?
			if [[ "$status" -eq 0 ]]; then
				idtech3_minimal_physics_smoke "$root" || status=$?
			fi
			if [[ "$status" -eq 0 ]]; then
				idtech3_minimal_app_crdt_smoke "$root" || status=$?
			fi
			if [[ "$status" -eq 0 ]]; then
				idtech3_minimal_oscar_smoke "$root" || status=$?
			fi
			;;
		*)
			echo "usage: $0 [all|map|physics|app_crdt|oscar]" >&2
			return 2
			;;
	esac

	if [[ "$status" -eq 77 ]]; then
		return 0
	fi
	return "$status"
}

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
	idtech3_minimal_run_case "${1:-all}"
fi
