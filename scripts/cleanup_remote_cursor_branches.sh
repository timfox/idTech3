#!/usr/bin/env bash
# Delete stale origin/cursor/* branches already merged into main (0 commits ahead).
# Use DELETE_ALL_CURSOR=1 to delete every origin/cursor/* branch (after integration).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

BASE="${CLEANUP_BASE_BRANCH:-main}"
DRY_RUN="${DRY_RUN:-0}"
DELETE_ALL="${DELETE_ALL_CURSOR:-0}"

if ! git rev-parse --verify "$BASE" >/dev/null 2>&1; then
	echo "Base branch $BASE not found" >&2
	exit 1
fi

git fetch origin --prune

deleted=0
kept=0

while read -r b; do
	short="${b#origin/}"
	ahead=$(git rev-list --count "$BASE..$b" 2>/dev/null || echo 0)
	if [ "$DELETE_ALL" != "1" ] && [ "$ahead" -gt 0 ]; then
		echo "KEEP $short (ahead $BASE by $ahead)"
		kept=$((kept + 1))
		continue
	fi
	if [ "$DRY_RUN" = "1" ]; then
		echo "DRY-RUN delete $short"
	else
		git push origin --delete "$short"
		echo "DELETED $short"
	fi
	deleted=$((deleted + 1))
done < <(git branch -r | grep 'origin/cursor/' | sed 's/^ *//')

echo "cleanup: deleted=$deleted kept=$kept (DELETE_ALL_CURSOR=$DELETE_ALL DRY_RUN=$DRY_RUN)"
