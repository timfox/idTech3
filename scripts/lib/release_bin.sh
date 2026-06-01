#!/usr/bin/env bash
# Shared helpers for scanning idtech3 release binaries (Linux ELF, Windows PE, macOS).
# Source from validation scripts:  source "$(dirname "$0")/lib/release_bin.sh"

release_bin_path() {
	local release_dir="$1"
	local bin="$2"
	local base="$release_dir/$bin"
	for candidate in \
		"$base" "$base.exe" \
		"$base.x64" "$base.x64.exe" "$base.x86_64" "$base.x86_64.exe" \
		"$base.aarch64" "$base.arm" "$base.armv7l" \
		"$base.aarch64.app/Contents/MacOS/$bin.aarch64" \
		"$base.aarch64.app/Contents/MacOS/$bin" \
		"$base.arm.app/Contents/MacOS/$bin.arm" \
		"$base.arm.app/Contents/MacOS/$bin" \
		"$base.app/Contents/MacOS/$bin"; do
		if [ -f "$candidate" ]; then
			echo "$candidate"
			return
		fi
	done
	echo ""
}

release_bin_has_text() {
	local bin="$1"
	local pattern="$2"

	[ -f "$bin" ] || return 1

	if grep -a -q -E "$pattern" "$bin" 2>/dev/null; then
		return 0
	fi
	if grep -q -E "$pattern" < <(strings -a "$bin" 2>/dev/null); then
		return 0
	fi
	if grep -q -E "$pattern" < <(strings -el "$bin" 2>/dev/null); then
		return 0
	fi
	return 1
}
