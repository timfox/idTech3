#!/usr/bin/env bash
# Print merge-readiness summary for the current engine branch (local + optional GitHub).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BRANCH="${1:-$(git -C "$ROOT" rev-parse --abbrev-ref HEAD 2>/dev/null || echo HEAD)}"

cd "$ROOT"

echo "=== PR readiness: $BRANCH vs main ==="
echo "Commits ahead: $(git rev-list --count "origin/main..HEAD" 2>/dev/null || echo '?')"
echo ""
echo "Local gates (run from repo root):"
echo "  ./scripts/resolve_renderer_tiers.sh --tier A"
echo "  GAME_BASE=/abs/path/to/base ./scripts/resolve_renderer_tiers.sh --strict --tier B"
echo "  cd build-vk-Release && ctest --output-on-failure"
echo "  ./scripts/validate_ci_build.sh"
echo "  ./scripts/evidence_status.sh"
echo ""
echo "Docs: docs/renderer_validation/README.md"
echo ""

if command -v gh >/dev/null 2>&1; then
	gh pr view --json state,mergeable,url,title -q '"\(.title)\nState: \(.state) mergeable=\(.mergeable)\n\(.url)"' 2>/dev/null || true
	echo ""
	gh pr checks 2>&1 | awk '
		/pending|in_progress|queued/ { p++ }
		/pass|success/ { ok++ }
		/fail/ { bad++ }
		END { printf "GitHub checks: pass/success=%d pending=%d fail=%d\n", ok+0, p+0, bad+0 }
	' || true
fi
