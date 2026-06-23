#!/usr/bin/env bash
# Tag tips of legacy origin branches, push tags, then delete the remote branches.
# Preserves audit history for enterprise/AAA trunk policy without keeping 10+ stale remotes.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

DRY_RUN="${DRY_RUN:-0}"
DATE_STAMP="${ARCHIVE_DATE:-$(date +%Y%m%d)}"
BASE="${ARCHIVE_BASE_BRANCH:-main}"

# Remotes to archive (never delete origin/main).
DEFAULT_LEGACY=(
	archive
	cherry
	chocolate
	duktape-linear
	feature/vulkan-rtx-demo-ray-pass
	glints
	gx-renderer
	next-gen-5
	overbaked
	vanilla
)

if [ -n "${ARCHIVE_BRANCHES:-}" ]; then
	# shellcheck disable=SC2206
	LEGACY=(${ARCHIVE_BRANCHES})
else
	LEGACY=("${DEFAULT_LEGACY[@]}")
fi

git fetch origin --prune

if ! git rev-parse --verify "$BASE" >/dev/null 2>&1; then
	echo "Base branch $BASE not found" >&2
	exit 1
fi

tagged=0
deleted=0
skipped=0

for short in "${LEGACY[@]}"; do
	ref="origin/$short"
	if ! git show-ref --verify --quiet "refs/remotes/$ref"; then
		echo "SKIP missing $ref"
		skipped=$((skipped + 1))
		continue
	fi

	tip="$(git rev-parse "$ref")"
	safe_name="$(echo "$short" | tr '/' '-')"
	tag="archive/${safe_name}-${DATE_STAMP}"
	desc="Archived tip of $ref ($tip) before trunk consolidation on $DATE_STAMP. Superseded by origin/main."

	if git show-ref --verify --quiet "refs/tags/$tag"; then
		echo "SKIP tag exists $tag"
	else
		if [ "$DRY_RUN" = "1" ]; then
			echo "DRY-RUN tag $tag -> $tip"
		else
			git tag -a "$tag" -m "$desc" "$tip"
			git push origin "$tag"
			echo "TAGGED $tag -> $tip"
		fi
		tagged=$((tagged + 1))
	fi

	if [ "$DRY_RUN" = "1" ]; then
		echo "DRY-RUN delete $short"
	else
		git push origin --delete "$short"
		echo "DELETED origin/$short"
	fi
	deleted=$((deleted + 1))
done

echo "archive_legacy: tagged=$tagged deleted=$deleted skipped=$skipped DRY_RUN=$DRY_RUN"
echo "Active trunk: origin/main only. Restore a legacy tip: git checkout archive/<name>-${DATE_STAMP}"
