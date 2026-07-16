#!/usr/bin/env bash
# Codebase-wide debt ratchet: block new first-party TODOs, stale src links,
# duplicate generated shader blobs, and raw unsafe C string calls.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

failures=0

fail_section() {
	local title="$1"
	local body="$2"
	echo "FAIL: ${title}" >&2
	if [[ -n "$body" ]]; then
		echo "$body" >&2
	fi
	failures=$((failures + 1))
}

require_tool() {
	command -v "$1" >/dev/null 2>&1 || {
		echo "FAIL: missing required tool '$1'" >&2
		exit 1
	}
}

require_tool rg

first_party_roots=(engine runtime modules renderers extensions)
source_globs=(
	--glob '*.{c,h,cpp,hpp,cc,cxx}'
	--glob '!renderers/vulkan/json.hpp'
	--glob '!third_party/**'
	--glob '!**/external/**'
)

todo_hits="$(rg -n '\b(TODO|FIXME)\b' "${first_party_roots[@]}" "${source_globs[@]}" || true)"
if [[ -n "$todo_hits" ]]; then
	fail_section "first-party source must not add TODO/FIXME comments" "$todo_hits"
fi

doc_link_hits="$(rg -n '\]\([^)]*(\.\./)?src/' README.md BUILD.md CHANGELOG.md docs --glob '*.md' || true)"
if [[ -n "$doc_link_hits" ]]; then
	filtered_doc_hits=""
	while IFS= read -r line; do
		file="${line%%:*}"
		case "$file" in
			docs/core/LEGACY_AND_MODERN.md|\
			docs/core/REPOSITORY_LAYOUT_2026.md|\
			docs/core/SHIM_REMOVAL_CHECKLIST.md|\
			docs/DEPRECATION_POLICY.md|\
			docs/ENGINE_REORG_PLAN.md|\
			docs/TODO_TRIAGE.md|\
			docs/CODEBASE_AUDIT_PHASE2.md)
				;;
			*)
				filtered_doc_hits+="${line}"$'\n'
				;;
		esac
	done <<< "$doc_link_hits"
	if [[ -n "$filtered_doc_hits" ]]; then
		fail_section "stale Markdown links to src/ outside approved legacy docs" "$filtered_doc_hits"
	fi
fi

if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
	generated_tracked="$(git ls-files 'renderers/vulkan/shaders/spirv/generated/*' 'src/renderers/vulkan/shaders/spirv/generated/*')"
	if [[ -n "$generated_tracked" ]]; then
		fail_section "generated Vulkan shader cache must not be tracked" "$generated_tracked"
	fi
else
	echo "WARN: not in a git worktree; skipping tracked generated shader cache check" >&2
fi

raw_string_hits="$(rg -n '(^|[^A-Za-z0-9_])(strcpy|strcat|sprintf)\s*\(' "${first_party_roots[@]}" "${source_globs[@]}" || true)"
if [[ -n "$raw_string_hits" ]]; then
	raw_string_violations=""
	while IFS= read -r line; do
		file="${line%%:*}"
		case "$file" in
			engine/platform/unix/unix_main.c|\
			engine/platform/win32/win_shared.c|\
			engine/platform/win32/win_syscon.c|\
			engine/platform/win32/botlib/*|\
			modules/botlib/*|\
			runtime/client/core/cl_cgame.c)
				;;
			*)
				raw_string_violations+="${line}"$'\n'
				;;
		esac
	done <<< "$raw_string_hits"
	if [[ -n "$raw_string_violations" ]]; then
		fail_section "new raw strcpy/strcat/sprintf sites outside legacy allowlist" "$raw_string_violations"
	fi
fi

if [[ "$failures" -ne 0 ]]; then
	echo "tech_debt_audit: ${failures} check group(s) failed" >&2
	exit 1
fi

echo "tech_debt_audit: passed"
