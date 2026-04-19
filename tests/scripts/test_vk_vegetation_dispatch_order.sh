#!/usr/bin/env bash
# Regression test for Vulkan vegetation wind dispatch ordering.
# Ensures dispatch happens after SURF_VEGETATION batches in RB_EndSurface,
# and is not dispatched early from vk_begin_frame.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
TR_SHADE="$PROJECT_ROOT/src/renderers/vulkan/tr_shade.c"
VK_FRAME_SUBMIT="$PROJECT_ROOT/src/renderers/vulkan/vk_frame_submit.c"

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

[ -f "$TR_SHADE" ] || fail "missing source file: $TR_SHADE"
[ -f "$VK_FRAME_SUBMIT" ] || fail "missing source file: $VK_FRAME_SUBMIT"
command -v python3 >/dev/null 2>&1 || fail "python3 not found in PATH"

python3 - "$TR_SHADE" "$VK_FRAME_SUBMIT" <<'PY'
import sys


def fail(msg: str) -> None:
    print(f"FAIL: {msg}", file=sys.stderr)
    raise SystemExit(1)


def extract_function_body(text: str, function_name: str) -> str:
    signature_idx = text.find(function_name)
    if signature_idx == -1:
        fail(f"function signature not found: {function_name}")

    brace_open = text.find("{", signature_idx)
    if brace_open == -1:
        fail(f"opening brace not found for function: {function_name}")

    depth = 0
    for idx in range(brace_open, len(text)):
        ch = text[idx]
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return text[brace_open + 1 : idx]

    fail(f"unterminated function body for: {function_name}")
    return ""


tr_shade_path = sys.argv[1]
vk_frame_submit_path = sys.argv[2]

with open(tr_shade_path, "r", encoding="utf-8") as f:
    tr_shade_text = f.read()
with open(vk_frame_submit_path, "r", encoding="utf-8") as f:
    vk_frame_text = f.read()

rb_end_surface_body = extract_function_body(tr_shade_text, "void RB_EndSurface( void )")
vk_begin_frame_body = extract_function_body(vk_frame_text, "void vk_begin_frame( void )")

required_fragments = [
    "PostFX_VegWind_IsEnabled()",
    "tess.shader->surfaceFlags & SURF_VEGETATION",
    "vk_vegetation_wind_dispatch();",
    "vk_vegetation_clear_staging();",
]
for fragment in required_fragments:
    if fragment not in rb_end_surface_body:
        fail(
            "RB_EndSurface missing required vegetation dispatch fragment: "
            f"{fragment}"
        )

if "vk_vegetation_wind_dispatch();" in vk_begin_frame_body:
    fail("vk_begin_frame still dispatches vegetation wind too early")

print("PASS: test_vk_vegetation_dispatch_order")
PY
