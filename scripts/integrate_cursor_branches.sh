#!/usr/bin/env bash
# Cherry-pick unique commits from origin/cursor/* branches onto the current branch.
# Skips duplicates and empty picks; continues on conflicts (--skip).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

BASE_BRANCH="${INTEGRATE_BASE_BRANCH:-main}"
TARGET_BRANCH="${INTEGRATE_TARGET_BRANCH:-integrate/cursor-branches}"
LOG="${INTEGRATE_LOG:-$ROOT/build-integrate-cursor.log}"

if ! git rev-parse --verify "$BASE_BRANCH" >/dev/null 2>&1; then
	echo "Base branch $BASE_BRANCH not found" >&2
	exit 1
fi

if git show-ref --verify --quiet "refs/heads/$TARGET_BRANCH"; then
	git checkout "$TARGET_BRANCH"
	git reset --hard "$BASE_BRANCH"
else
	git checkout -b "$TARGET_BRANCH" "$BASE_BRANCH"
fi

: >"$LOG"
picked=0
skipped=0
failed=0

MIN_YEAR="${INTEGRATE_MIN_YEAR:-2025}"

pick_commit() {
	local h="$1"
	local subj year
	subj="$(git log -1 --format='%s' "$h")"
	year="$(git log -1 --format='%ci' "$h" | cut -c1-4)"
	if [ "$year" -lt "$MIN_YEAR" ]; then
		echo "SKIP old   $h ($year) $subj" | tee -a "$LOG"
		skipped=$((skipped + 1))
		return 0
	fi
	if git merge-base --is-ancestor "$h" HEAD; then
		echo "SKIP ancestor $h $subj" | tee -a "$LOG"
		skipped=$((skipped + 1))
		return 0
	fi
	if git cherry-pick -x "$h" >>"$LOG" 2>&1; then
		echo "OK   $h $subj" | tee -a "$LOG"
		picked=$((picked + 1))
		return 0
	fi
	echo "FAIL $h $subj (cherry-pick conflict — skipping)" | tee -a "$LOG"
	git cherry-pick --abort 2>/dev/null || git reset --hard HEAD
	failed=$((failed + 1))
	return 0
}

echo "Collecting unique commits from origin/cursor/* not in $BASE_BRANCH..."
mapfile -t branches < <(git branch -r | grep 'origin/cursor/' | sed 's/^ *//' | sort)
declare -A seen

for b in "${branches[@]}"; do
	ahead=$(git rev-list --count "$BASE_BRANCH..$b" 2>/dev/null || echo 0)
	if [ "$ahead" -eq 0 ]; then
		continue
	fi
	mapfile -t commits < <(git rev-list --reverse "$BASE_BRANCH..$b")
	for h in "${commits[@]}"; do
		if [[ -n "${seen[$h]:-}" ]]; then
			continue
		fi
		seen[$h]=1
		pick_commit "$h"
	done
done

echo "=== integrate_cursor_branches summary ===" | tee -a "$LOG"
echo "picked=$picked skipped=$skipped failed=$failed" | tee -a "$LOG"
echo "Branch: $TARGET_BRANCH (reset from $BASE_BRANCH if re-run)" | tee -a "$LOG"
echo "Log: $LOG" | tee -a "$LOG"

git checkout "$BASE_BRANCH"
