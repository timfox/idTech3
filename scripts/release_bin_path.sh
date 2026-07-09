#!/usr/bin/env bash
# Resolve staged CI/release binary paths (arch suffixes, Windows .exe, macOS bundles).
# Usage: release_bin_path <release_dir> <basename>
# Prints the resolved path or nothing when not found.
release_bin_path() {
	local release_dir="$1"
	local bin="$2"
	local base="$release_dir/$bin"
	local candidate

	for candidate in \
		"$base" "$base.exe" \
		"$base.x64" "$base.x64.exe" "$base.x86_64" "$base.x86_64.exe" \
		"$base.aarch64" "$base.arm" "$base.armv7" "$base.armv7l" \
		"$base.aarch64.app/Contents/MacOS/$bin.aarch64" \
		"$base.aarch64.app/Contents/MacOS/$bin" \
		"$base.arm.app/Contents/MacOS/$bin.arm" \
		"$base.arm.app/Contents/MacOS/$bin" \
		"$base.app/Contents/MacOS/$bin"; do
		if [ -f "$candidate" ]; then
			echo "$candidate"
			return 0
		fi
	done

	return 1
}
