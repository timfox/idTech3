#!/usr/bin/env bash
# Trunk policy: origin should expose only main; no cursor/* remotes; archive tags + scripts present.
set -euo pipefail

ROOT="$(cd "$(dirname "${0}")/../.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

if [ "${SKIP_TRUNK_FETCH:-0}" != "1" ]; then
	if ! command -v git >/dev/null 2>&1; then
		fail "git not in PATH (set SKIP_TRUNK_FETCH=1 to skip)"
	fi
	git fetch origin --prune --tags 2>/dev/null || fail "git fetch origin failed (set SKIP_TRUNK_FETCH=1 to skip)"
fi

# --- required governance files ---
[ -f "${ROOT}/docs/BRANCHES.md" ] || fail "missing docs/BRANCHES.md"
[ -f "${ROOT}/.github/dependabot.yml" ] || fail "missing .github/dependabot.yml"
[ -x "${ROOT}/scripts/archive_legacy_remote_branches.sh" ] || fail "missing archive_legacy_remote_branches.sh"
[ -x "${ROOT}/scripts/cleanup_remote_cursor_branches.sh" ] || fail "missing cleanup_remote_cursor_branches.sh"

if ! grep -q 'feature/\*' "${ROOT}/docs/BRANCHES.md"; then
	fail "docs/BRANCHES.md missing feature/* trunk guidance"
fi
pass "trunk governance files present"

# --- remote branch hygiene ---
if [ "${GITHUB_ACTIONS:-}" = "true" ] && [ "${ALLOW_EXTRA_ORIGIN_REFS_IN_CI:-1}" = "1" ]; then
	if [ "${SKIP_TRUNK_FETCH:-0}" = "1" ]; then
		pass "skipping origin/main check (SKIP_TRUNK_FETCH)"
	elif ! command -v git >/dev/null 2>&1; then
		pass "skipping origin/main check (git not in PATH)"
	elif git branch -r | sed 's/^ *//' | grep -qx 'origin/main'; then
		pass "origin/main visible in CI checkout"
	else
		pass "origin/main not in shallow CI checkout (acceptable)"
	fi
	pass "skipping strict remote hygiene in GitHub Actions checkout"
else
	mapfile -t remotes < <(git branch -r | sed 's/^ *//' | grep -v 'origin/HEAD' || true)
	bad=()
	allowed_only_main=1

	for ref in "${remotes[@]}"; do
		case "$ref" in
			origin/main) continue ;;
			origin/cursor/*) bad+=("$ref"); continue ;;
			*) bad+=("$ref"); allowed_only_main=0 ;;
		esac
	done

	if [ "${#bad[@]}" -gt 0 ]; then
		echo "Unexpected origin remotes (trunk policy: only origin/main):" >&2
		printf '  %s\n' "${bad[@]}" >&2
		fail "stale remote branches detected"
	fi
	pass "origin has only main (plus HEAD)"

	cursor_count="$(git branch -r | grep -c 'origin/cursor/' || true)"
	if [ "$cursor_count" -gt 0 ]; then
		fail "found $cursor_count origin/cursor/* remote(s)"
	fi
	pass "no origin/cursor/* remotes"
fi

# --- archive tags (legacy consolidation) ---
archive_count="$(git tag -l 'archive/*' 2>/dev/null | wc -l | tr -d ' ')"
if [ "${SKIP_TRUNK_FETCH:-0}" = "1" ]; then
	pass "skipping archive/* tag check (SKIP_TRUNK_FETCH)"
elif [ "${REQUIRE_ARCHIVE_TAGS:-1}" = "1" ] && [ "$archive_count" -lt 1 ]; then
	fail "expected archive/* tags (run scripts/archive_legacy_remote_branches.sh once)"
else
	pass "archive/* tags present ($archive_count)"
fi

# --- local integration scripts wired in tests ---
[ -x "${ROOT}/tests/scripts/test_legacy_intact.sh" ] || fail "missing test_legacy_intact.sh"
./tests/scripts/test_legacy_intact.sh

echo "test_trunk_policy: passed"
