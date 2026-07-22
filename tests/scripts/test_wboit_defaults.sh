#!/usr/bin/env bash
# Gate: production WBOIT (r_oit 1) on shipping profiles; MBOIT (r_oit 2) only experimental.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
failures=0

check_oit() {
	local file="$1"
	local expect="$2"
	local label="$3"
	if [[ ! -f "$file" ]]; then
		echo "FAIL: missing $file ($label)"
		failures=$((failures + 1))
		return
	fi
	if ! grep -Eq "seta[[:space:]]+r_oit[[:space:]]+${expect}" "$file"; then
		echo "FAIL: $label — expected seta r_oit $expect in $file"
		failures=$((failures + 1))
	else
		echo "PASS: $label (r_oit $expect)"
	fi
}

check_not_oit() {
	local file="$1"
	local bad="$2"
	local label="$3"
	if [[ ! -f "$file" ]]; then
		echo "FAIL: missing $file ($label)"
		failures=$((failures + 1))
		return
	fi
	if grep -Eq "seta[[:space:]]+r_oit[[:space:]]+${bad}" "$file"; then
		echo "FAIL: $label — must not set r_oit $bad in $file"
		failures=$((failures + 1))
	else
		echo "PASS: $label (not r_oit $bad)"
	fi
}

check_oit "$ROOT/config/modern_vulkan_stable.cfg" 1 "stable production WBOIT"
check_oit "$ROOT/config/modern_vulkan_quality.cfg" 1 "quality production WBOIT"
check_oit "$ROOT/config/modern_clustered.cfg" 1 "clustered production WBOIT"
check_oit "$ROOT/config/vulkan_overlay_oit_clustered.cfg" 1 "oit_clustered overlay WBOIT"
check_oit "$ROOT/config/vulkan_overlay_spine_1_1_cert.cfg" 1 "Spine 1.1 cert WBOIT"

check_oit "$ROOT/config/modern_vulkan_experimental.cfg" 2 "experimental MBOIT"
check_oit "$ROOT/config/vulkan_overlay_mboit.cfg" 2 "mboit overlay MBOIT"

check_not_oit "$ROOT/config/modern_vulkan_stable.cfg" 2 "stable must not prefer MBOIT"
check_not_oit "$ROOT/config/modern_clustered.cfg" 2 "clustered must not prefer MBOIT"
check_not_oit "$ROOT/config/vulkan_overlay_oit_clustered.cfg" 2 "oit_clustered must not prefer MBOIT"

if grep -q 'r_oit 2' "$ROOT/examples/demo_game/mod/demo_oit_clustered.cfg" 2>/dev/null; then
	echo "FAIL: demo_oit_clustered.cfg must not advertise r_oit 2"
	failures=$((failures + 1))
else
	echo "PASS: demo_oit_clustered.cfg does not advertise MBOIT"
fi

if [[ $failures -ne 0 ]]; then
	echo "$failures check(s) failed"
	exit 1
fi
echo "All WBOIT defaults checks passed."
