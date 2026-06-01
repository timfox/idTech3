#!/usr/bin/env bash
# Evidence gap report: Tier A/B/C/D vs what this machine/repo can run now.
# Does not upload secrets; read-only checks. Exit 0 (informational).
#
# Usage:
#   ./scripts/evidence_status.sh
#   GAME_BASE=/path/to/base ./scripts/evidence_status.sh
#
# See: docs/PRODUCTION_CERTIFICATION.md, docs/renderer_validation/README.md

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT"

pass() { printf '  [ok]   %s\n' "$*"; }
warn() { printf '  [warn] %s\n' "$*"; }
skip() { printf '  [skip] %s\n' "$*"; }
info() { printf '  [info] %s\n' "$*"; }

echo "=== Engine evidence status (idTech3) ==="
echo "Repo: $ROOT"
echo "Date: $(date -u +%Y-%m-%dT%H:%M:%SZ 2>/dev/null || date)"
if git rev-parse --short HEAD >/dev/null 2>&1; then
	echo "HEAD: $(git rev-parse --short HEAD)"
fi
echo ""

echo "Tier A - Automated (CI + local orchestrator)"
if [[ -d "$ROOT/build-vk-Release" ]]; then
	pass "build-vk-Release exists (run ctest / production_readiness.sh here)"
else
	warn "build-vk-Release missing - ./scripts/compile_engine.sh vulkan"
fi
if [[ -x "$ROOT/scripts/production_readiness.sh" ]]; then
	info "Run: ./scripts/production_readiness.sh  (full local gate; optional GAME_BASE)"
fi
if [[ -x "$ROOT/scripts/validate_ci_build.sh" ]]; then
	info "Run: ./scripts/validate_ci_build.sh  (CI-parity smoke)"
fi
if [[ -x "$ROOT/scripts/openarena_validate.sh" ]]; then
	info "Run: ./scripts/openarena_validate.sh release  (QVM compat + launcher + optional Tier B)"
fi
if [[ -f "$ROOT/docs/OPENARENA.md" ]]; then
	info "OpenArena playbook: docs/OPENARENA.md"
fi
if command -v gh >/dev/null 2>&1 && gh auth status >/dev/null 2>&1; then
	if gh api "repos/$(gh repo view --json nameWithOwner -q .nameWithOwner 2>/dev/null)/actions/workflows/build.yml" >/dev/null 2>&1; then
		info "GitHub CLI: check Actions UI for latest main matrix (this script does not poll CI)"
	fi
else
	skip "gh not logged in - open GitHub Actions for matrix status"
fi
echo ""

echo "Tier B - Content-backed (GAME_BASE regression)"
DEVDATA_BASE="$ROOT/docs/renderer_validation/devdata/rtest_base"
if [[ -n "${GAME_BASE:-}" ]]; then
	if [[ -d "$GAME_BASE" ]]; then
		pass "GAME_BASE is a directory: $GAME_BASE"
		if [[ -f "$ROOT/scripts/renderer_regression_maps.sh" ]]; then
			info "Run: GAME_BASE=\"\$GAME_BASE\" ./scripts/renderer_regression_maps.sh"
			info "Run: GAME_BASE=\"\$GAME_BASE\" ./scripts/renderer_regression_check.sh"
		fi
	else
		warn "GAME_BASE set but not a directory: $GAME_BASE"
	fi
elif [[ -f "$DEVDATA_BASE/vm/qagame.qvm" ]]; then
	pass "shipped devdata: $DEVDATA_BASE"
	info "Run: ./scripts/run_renderer_tier_b_devdata.sh  (no retail pk3)"
	info "Or:  export GAME_BASE=\"$DEVDATA_BASE\" && ./scripts/renderer_regression_maps.sh"
else
	skip "GAME_BASE unset and devdata missing qagame.qvm"
	info "Build devdata: ./scripts/build_renderer_devdata.sh (needs ioquake3 qagame.qvm)"
fi
info "GitHub Tier B (full game tree): IDTECH3_GAME_BASE_PATH + runner idtech3-tierb (docs/renderer_validation/SELF_HOSTED_TIER_B.md)"
info "GitHub Tier B (devdata only): ubuntu-x86_64 ctest includes renderer_regression_maps_devdata when qvm is in repo"
echo ""

echo "Tier C - Manual GPU / validation layers"
if [[ -f "$ROOT/docs/renderer_validation/TEMPLATE_TIER_C.md" ]]; then
	info "Copy docs/renderer_validation/TEMPLATE_TIER_C.md per session; append row to FINDINGS.md"
fi
if [[ -f "$ROOT/docs/renderer_validation/FINDINGS.md" ]]; then
	if grep -q 'Add a row when you complete a real GPU pass' "$ROOT/docs/renderer_validation/FINDINGS.md" 2>/dev/null; then
		warn "FINDINGS.md Tier C section still describes an empty log - add a real GPU pass row"
	else
		info "FINDINGS.md: review Tier C table for dated sessions"
	fi
fi
info "Follow renderer proof loop: docs/RENDERER_CONFIDENCE.md"
echo ""

echo "Tier D - Release hygiene"
if [[ -f "$ROOT/docs/RELEASE_CHECKLIST.md" ]]; then
	info "Before tag: docs/RELEASE_CHECKLIST.md"
fi
echo ""

echo "Title / AAA (out of engine repo)"
info "Game cert, telemetry, soak, platform submission: examples/title-repo/CERTIFICATION_CHECKLIST.md"
echo ""
echo "=== End evidence status ==="
