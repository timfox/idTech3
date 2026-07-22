#!/usr/bin/env bash
# Forbid Miles / Ritual SDK leakage and retail asset commits in ÜberTools paths.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

fail=0

check_absent() {
	local pat="$1"
	local desc="$2"
	if rg -n --glob '!third_party/**' --glob '!.git/**' -i "$pat" \
		modules/dialogue renderers/common/tr_model_tiki* renderers/vulkan/tr_model_tiki* \
		docs/UBERTOOLS_CLEAN_ROOM.md docs/BABBLE.md docs/TIKI.md \
		cmake/modules/UberToolsSources.cmake CMakeLists.txt 2>/dev/null | head -20 | grep -q .; then
		# Allow documentation that mentions Miles as excluded
		if [[ "$pat" == "Miles" || "$pat" == "mss\\.h" ]]; then
			if rg -n -i "$pat" docs/UBERTOOLS_CLEAN_ROOM.md docs/BABBLE.md docs/TIKI.md 2>/dev/null | grep -viE 'forbid|exclu|not |never|no Miles|proprietary' | grep -q .; then
				echo "FAIL: unexpected Miles reference outside exclusion prose ($desc)"
				fail=1
			fi
			return 0
		fi
		echo "FAIL: found pattern /$pat/ ($desc)"
		rg -n --glob '!third_party/**' -i "$pat" modules/dialogue renderers/common/tr_model_tiki* renderers/vulkan/tr_model_tiki* cmake/modules/UberToolsSources.cmake || true
		fail=1
	fi
}

# Hard forbid in sources / CMake
if rg -n --glob '!third_party/**' --glob '!.git/**' -i \
	'miles\.lib|libmiles|mss32|mss64|#include\s*[<"]mss\.h|USE_MILES|AIL_startup|MilesSound' \
	. 2>/dev/null | grep -v 'UBERTOOLS_CLEAN_ROOM\|test_ubertools_clean_room\|BABBLE\.md\|TIKI\.md\|forbid\|exclu' | head -5 | grep -q .; then
	echo "FAIL: Miles symbols/headers/options detected"
	fail=1
else
	echo "OK: no Miles integration markers"
fi

if rg -n 'Ritual.*SDK|tiki\.h\s*from\s*sdk|babble_sdk' modules/dialogue renderers --glob '!*.md' 2>/dev/null \
	| grep -viE 'not derived|not Ritual|never|exclu|forbid|clean-room|notes only' \
	| head -5 | grep -q .; then
	echo "FAIL: Ritual SDK markers in sources"
	fail=1
else
	echo "OK: no Ritual SDK source markers"
fi

# Fixtures must stay synthetic
if [[ -d tests/fixtures/ubertools ]]; then
	if find tests/fixtures/ubertools -type f \( -iname '*fakk*' -o -iname '*retail*' -o -iname '*mohaa*' \) | grep -q .; then
		echo "FAIL: retail-named fixtures under tests/fixtures/ubertools"
		fail=1
	else
		echo "OK: ubertools fixtures look synthetic"
	fi
fi

# Required policy doc
[[ -f docs/UBERTOOLS_CLEAN_ROOM.md ]] || { echo "FAIL: missing docs/UBERTOOLS_CLEAN_ROOM.md"; fail=1; }

if [[ "$fail" -ne 0 ]]; then
	echo "test_ubertools_clean_room: FAILED"
	exit 1
fi
echo "test_ubertools_clean_room: PASS"
exit 0
