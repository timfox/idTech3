#!/usr/bin/env bash
# Print merge-readiness summary for the Q3/OA Vulkan branch (local + optional GitHub).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BRANCH="${1:-cursor/fix-vulkan-q3-compat-check-d22f}"

cd "$ROOT"

echo "=== PR readiness: $BRANCH vs main ==="
echo "Commits ahead: $(git rev-list --count "origin/main..HEAD" 2>/dev/null || echo '?')"
echo ""
echo "Local gates (run from repo root):"
echo "  ./scripts/q3_openarena_compat_check.sh release"
echo "  ./scripts/run_renderer_tier_b_devdata.sh"
echo "  cd build-vk-Release && ctest -j1"
echo "  ./scripts/validate_ci_build.sh"
echo ""
echo "Docs: docs/Q3_OPENARENA_VULKAN.md"
echo "PR:   https://github.com/timfox/idTech3/pull/230"
echo ""

if command -v gh >/dev/null 2>&1; then
	gh pr view 230 --repo timfox/idTech3 --json state,mergeable,url,title -q '"\(.title)\nState: \(.state) mergeable=\(.mergeable)\n\(.url)"' 2>/dev/null || true
	echo ""
	gh pr checks 230 --repo timfox/idTech3 2>&1 | awk '
		/pending|in_progress|queued/ { p++ }
		/pass|success/ { ok++ }
		/fail/ { bad++ }
		END { printf "GitHub checks: pass/success=%d pending=%d fail=%d\n", ok+0, p+0, bad+0 }
	' || true
fi
