#!/usr/bin/env bash
# Regression test for Vulkan vegetation wind dispatch ordering.
# Ensures dispatch happens after vegetation tess staging upload and is not run
# from frame start where vertexCount would be zero.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TR_SHADE_FILE="${1:-$PROJECT_ROOT/src/renderers/vulkan/tr_shade.c}"
FRAME_SUBMIT_FILE="${2:-$PROJECT_ROOT/src/renderers/vulkan/vk_frame_submit.c}"

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

require_file() {
	local path="$1"
	if [ ! -f "$path" ]; then
		fail "missing file: $path"
	fi
}

extract_function_body() {
	local file="$1"
	local function_name="$2"
	local out_file="$3"

	awk -v function_name="$function_name" '
	BEGIN {
		in_signature = 0;
		in_body = 0;
		depth = 0;
		found = 0;
	}
	!in_signature && $0 ~ "^[[:space:]]*void[[:space:]]+" function_name "[[:space:]]*\\(" {
		in_signature = 1;
	}
	in_signature && !in_body {
		line = $0;
		open_count = gsub(/\{/, "{", line);
		if (open_count > 0) {
			in_body = 1;
			found = 1;
			close_count = gsub(/\}/, "}", line);
			depth += open_count - close_count;
			if (depth == 0) {
				exit;
			}
		}
		next;
	}
	in_body {
		print $0;
		line = $0;
		open_count = gsub(/\{/, "{", line);
		close_count = gsub(/\}/, "}", line);
		depth += open_count - close_count;
		if (depth == 0) {
			exit;
		}
	}
	END {
		if (!found) {
			exit 2;
		}
		if (depth != 0) {
			exit 3;
		}
	}
	' "$file" > "$out_file" || return 1
}

line_of() {
	local file="$1"
	local needle="$2"
	awk -v needle="$needle" 'index($0, needle) { print NR; exit }' "$file"
}

require_file "$TR_SHADE_FILE"
require_file "$FRAME_SUBMIT_FILE"

tmp_body="$(mktemp)"
trap 'rm -f "$tmp_body"' EXIT

if ! extract_function_body "$TR_SHADE_FILE" "RB_EndSurface" "$tmp_body"; then
	fail "could not extract RB_EndSurface body from $TR_SHADE_FILE"
fi

iterator_line="$(line_of "$tmp_body" "tess.shader->optimalStageIteratorFunc();")"
guard_line="$(line_of "$tmp_body" "PostFX_VegWind_IsEnabled() && tess.shader && ( tess.shader->surfaceFlags & SURF_VEGETATION )")"
dispatch_line="$(line_of "$tmp_body" "vk_vegetation_wind_dispatch();")"
clear_line="$(line_of "$tmp_body" "vk_vegetation_clear_staging();")"

if [ -z "$iterator_line" ]; then
	fail "RB_EndSurface missing tess.shader->optimalStageIteratorFunc()"
fi
if [ -z "$guard_line" ]; then
	fail "RB_EndSurface missing vegetation guard with PostFX_VegWind_IsEnabled + SURF_VEGETATION"
fi
if [ -z "$dispatch_line" ]; then
	fail "RB_EndSurface missing vk_vegetation_wind_dispatch() call"
fi
if [ -z "$clear_line" ]; then
	fail "RB_EndSurface missing vk_vegetation_clear_staging() call"
fi

if ! [ "$iterator_line" -lt "$guard_line" ] || ! [ "$guard_line" -lt "$dispatch_line" ] || ! [ "$dispatch_line" -lt "$clear_line" ]; then
	fail "RB_EndSurface ordering regression: expected stage iterator -> guard -> dispatch -> clear staging"
fi

if awk 'index($0, "vk_vegetation_wind_dispatch();") { exit 0 } END { exit 1 }' "$FRAME_SUBMIT_FILE"; then
	fail "vk_frame_submit.c still dispatches vegetation wind from frame-start path"
fi

echo "PASS: test_vk_vegetation_dispatch_order"
