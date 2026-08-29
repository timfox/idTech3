#!/usr/bin/env bash
# Guard against reintroducing compile-time flags that are always on in the Vulkan renderer.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# shellcheck source=idtech3_test_paths.sh
source "$SCRIPT_DIR/idtech3_test_paths.sh"
idtech3_test_paths_init "$PROJECT_ROOT"

VK_ROOT="$IDTECH3_RENDERERS/vulkan"

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

STALE_PATTERNS=(
	'^[[:space:]]*#[[:space:]]*ifdef[[:space:]]+USE_VBO_GRID\b'
	'^[[:space:]]*#[[:space:]]*ifdef[[:space:]]+USE_TESS_NEEDS_NORMAL\b'
	'^[[:space:]]*#[[:space:]]*ifdef[[:space:]]+USE_TESS_NEEDS_ST2\b'
	'^[[:space:]]*#[[:space:]]*ifdef[[:space:]]+USE_PMLIGHT\b'
	'^[[:space:]]*#[[:space:]]*ifndef[[:space:]]+USE_DEDICATED_ALLOCATION\b'
	'^[[:space:]]*#[[:space:]]*ifdef[[:space:]]+USE_VBO\b'
	'^[[:space:]]*#[[:space:]]*ifdef[[:space:]]+USE_FOG_COLLAPSE\b'
	'^[[:space:]]*#[[:space:]]*ifdef[[:space:]]+USE_VK_PBR\b'
	'^[[:space:]]*#[[:space:]]*ifndef[[:space:]]+USE_VK_PBR\b'
	'^[[:space:]]*#[[:space:]]*ifndef[[:space:]]+USE_VULKAN\b'
	'^[[:space:]]*#[[:space:]]*ifdef[[:space:]]+USE_VULKAN\b'
	'^[[:space:]]*#[[:space:]]*ifdef[[:space:]]+VK_CUBEMAP\b'
	'^[[:space:]]*#[[:space:]]*ifdef[[:space:]]+VK_PBR_BRDFLUT\b'
)

for pat in "${STALE_PATTERNS[@]}"; do
	if grep -REq "$pat" "$VK_ROOT" --include='*.c' --include='*.h' --include='*.cpp' --include='*.hpp' --include='*.inc'; then
		grep -REn "$pat" "$VK_ROOT" --include='*.c' --include='*.h' --include='*.cpp' --include='*.hpp' --include='*.inc' >&2 || true
		fail "stale compile-time guard matched: $pat"
	fi
done

# Orphan #endif scan (unwrap regressions).
if ! python3 - "$VK_ROOT" <<'PY'
import re, sys
from pathlib import Path
root = Path(sys.argv[1])
bad = False
for p in sorted(root.rglob('*')):
    if p.suffix not in {'.c', '.h', '.cpp', '.hpp', '.inc'}:
        continue
    stack = []
    for i, line in enumerate(p.read_text().splitlines(), 1):
        if re.match(r'^\s*#\s*if(?:def|ndef)?\b', line) or re.match(r'^\s*#\s*if\s+', line):
            stack.append(i)
        elif re.match(r'^\s*#\s*endif\b', line):
            if stack:
                stack.pop()
            else:
                print(f"orphan #endif at {p}:{i}")
                bad = True
    if stack:
        print(f"unclosed #if at {p}:{stack[-1]}")
        bad = True
sys.exit(1 if bad else 0)
PY
then
	fail "preprocessor balance check failed under $VK_ROOT"
fi

echo "PASS: test_stale_compile_flags"
