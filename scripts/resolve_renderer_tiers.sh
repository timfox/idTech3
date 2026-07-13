#!/usr/bin/env bash
# Resolve renderer validation tiers A-D as far as this machine can prove.
# Default mode is developer-friendly: run available automated checks and report skips.
# Strict mode is release/CI-friendly: missing Tier B/C/D evidence is a failure.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

STRICT=0
TIER="all"
SUMMARY_FILE=""
STATUS_RECORDS=""

usage() {
	cat <<'EOF'
Usage: ./scripts/resolve_renderer_tiers.sh [--strict] [--tier A|B|C|D|all] [--summary-file path]

Environment:
  GAME_BASE=/abs/path/to/base       Enables Tier B content-backed checks.
  RELEASE_DIR=/abs/path/to/release  Overrides release artifact/cfg lookup.
  BUILD_DIR=/abs/path/to/build      Overrides client lookup for runtime smoke.
  IDTECH3_BIN=/abs/path/to/idtech3  Overrides client lookup for runtime smoke.

Default mode exits 0 when unavailable hardware/content causes a documented skip.
--strict exits non-zero when required tier evidence is missing.
--summary-file writes a small JSON report for CI artifacts.
EOF
}

while [[ $# -gt 0 ]]; do
	case "$1" in
		--strict) STRICT=1; shift ;;
		--summary-file)
			[[ $# -ge 2 ]] || { usage >&2; exit 2; }
			SUMMARY_FILE="$2"
			shift 2
			;;
		--tier)
			[[ $# -ge 2 ]] || { usage >&2; exit 2; }
			TIER="$2"
			shift 2
			;;
		-h|--help)
			usage
			exit 0
			;;
		*)
			echo "Unknown argument: $1" >&2
			usage >&2
			exit 2
			;;
	esac
done

case "$TIER" in
	A|a) TIER="A" ;;
	B|b) TIER="B" ;;
	C|c) TIER="C" ;;
	D|d) TIER="D" ;;
	all|ALL) TIER="all" ;;
	*) echo "Unknown tier: $TIER" >&2; exit 2 ;;
esac

failures=0

json_escape() {
	local value="$1"
	value="${value//\\/\\\\}"
	value="${value//\"/\\\"}"
	value="${value//$'\n'/\\n}"
	printf '%s' "$value"
}

record_status() {
	local tier="$1"
	local status="$2"
	local message="$3"
	local entry
	entry="$(printf '{"tier":"%s","status":"%s","message":"%s"}' \
		"$(json_escape "$tier")" \
		"$(json_escape "$status")" \
		"$(json_escape "$message")")"
	if [[ -n "$STATUS_RECORDS" ]]; then
		STATUS_RECORDS="${STATUS_RECORDS},${entry}"
	else
		STATUS_RECORDS="$entry"
	fi
}

note() { printf '[info] %s\n' "$*"; }
pass() {
	printf '[ok]   %s\n' "$*"
	record_status "${CURRENT_TIER:-general}" "ok" "$*"
}
warn() {
	printf '[warn] %s\n' "$*"
	record_status "${CURRENT_TIER:-general}" "warn" "$*"
}
fail() {
	printf '[fail] %s\n' "$*" >&2
	record_status "${CURRENT_TIER:-general}" "fail" "$*"
	failures=$((failures + 1))
}

run_step() {
	local label="$1"
	shift
	note "$label"
	if "$@"; then
		pass "$label"
	else
		fail "$label"
	fi
}

has_real_tier_c_finding() {
	local findings="$ROOT/docs/renderer_validation/FINDINGS.md"
	[[ -f "$findings" ]] || return 1
	awk '
		/^## Tier A/ { in_c = 0 }
		/^## Tier C/ { in_c = 1; next }
		in_c && /^\|/ && $0 !~ /^\| Date/ && $0 !~ /^\|[- ]+\|/ && $0 !~ /\| - \| - \| - \| - \|/ { found = 1 }
		END { exit(found ? 0 : 1) }
	' "$findings"
}

tier_a() {
	CURRENT_TIER="A"
	echo "=== Tier A: automated/headless ==="
	run_step "renderer regression source/GLSL contract" ./scripts/renderer_regression_check.sh
	run_step "GPU golden manifest placeholder/size contract" ./scripts/gpu_golden_capture.sh --compare
	run_step "modern renderer profile source contract" ./tests/scripts/test_modern_renderer_profile_runtime.sh source

	if [[ -d "$ROOT/build-vk-Release" ]]; then
		run_step "selected renderer CTest contracts" \
			ctest --test-dir "$ROOT/build-vk-Release" -R 'renderer_regression_check|gpu_golden_compare|test_modern_vulkan_default|test_deferred_lighting|test_modern_renderer_profile_runtime' --output-on-failure
	else
		if [[ "$STRICT" -eq 1 ]]; then
			fail "build-vk-Release missing; strict Tier A requires configured build dir"
		else
			warn "build-vk-Release missing; skipped selected CTest contracts"
		fi
	fi
}

tier_b() {
	CURRENT_TIER="B"
	echo "=== Tier B: content-backed/runtime ==="
	if [[ -n "${GAME_BASE:-}" && -d "${GAME_BASE:-}" ]]; then
		run_step "renderer regression source contract with GAME_BASE" env GAME_BASE="$GAME_BASE" ./scripts/renderer_regression_check.sh
		run_step "renderer regression dedicated map loads" env GAME_BASE="$GAME_BASE" ./scripts/renderer_regression_maps.sh
	else
		if [[ "$STRICT" -eq 1 ]]; then
			fail "GAME_BASE is unset or not a directory; strict Tier B requires content-backed checks"
		else
			warn "GAME_BASE unavailable; skipped content-backed map checks"
		fi
	fi

	if [[ "$STRICT" -eq 1 ]]; then
		run_step "modern renderer profile client runtime smoke" env \
			IDTECH3_RENDERER_RUNTIME_REQUIRED=1 \
			IDTECH3_REQUIRE_RELEASE_CFGS=1 \
			./tests/scripts/test_modern_renderer_profile_runtime.sh all
	else
		run_step "modern renderer profile client runtime smoke when display/Vulkan are available" \
			./tests/scripts/test_modern_renderer_profile_runtime.sh all
	fi
}

tier_c() {
	CURRENT_TIER="C"
	echo "=== Tier C: manual GPU evidence ==="
	if has_real_tier_c_finding; then
		pass "Tier C findings contain at least one real GPU/validation row"
	else
		if [[ "$STRICT" -eq 1 ]]; then
			fail "Tier C findings are empty; record a real GPU pass in docs/renderer_validation/FINDINGS.md"
		else
			warn "Tier C findings are empty; use docs/renderer_validation/TEMPLATE_TIER_C.md after a GPU pass"
		fi
	fi
	note "Manual loop: docs/RENDERER_CONFIDENCE.md"
}

tier_d() {
	CURRENT_TIER="D"
	echo "=== Tier D: release hygiene ==="
	[[ -f "$ROOT/docs/RELEASE_CHECKLIST.md" ]] && pass "release checklist exists" || fail "docs/RELEASE_CHECKLIST.md missing"
	[[ -f "$ROOT/docs/PRODUCTION_CERTIFICATION.md" ]] && pass "production certification doc exists" || fail "docs/PRODUCTION_CERTIFICATION.md missing"
	[[ -x "$ROOT/scripts/evidence_status.sh" ]] && pass "evidence status script exists" || fail "scripts/evidence_status.sh missing or not executable"

	local release_dir="${RELEASE_DIR:-$ROOT/release}"
	if [[ -d "$release_dir" ]]; then
		pass "release directory exists: $release_dir"
		if [[ -x "$release_dir/idtech3_server" || -f "$release_dir/idtech3_server.exe" ]]; then
			pass "release has dedicated server artifact"
		elif [[ "$STRICT" -eq 1 ]]; then
			fail "release missing dedicated server artifact"
		else
			warn "release missing dedicated server artifact"
		fi
	else
		if [[ "$STRICT" -eq 1 ]]; then
			fail "release directory missing"
		else
			warn "release directory missing; run ./scripts/compile_engine.sh vulkan before release"
		fi
	fi
}

echo "=== Renderer tier resolver ==="
echo "Root: $ROOT"
echo "Tier: $TIER"
echo "Strict: $STRICT"
echo ""

if [[ "$TIER" == "all" || "$TIER" == "A" ]]; then tier_a; echo ""; fi
if [[ "$TIER" == "all" || "$TIER" == "B" ]]; then tier_b; echo ""; fi
if [[ "$TIER" == "all" || "$TIER" == "C" ]]; then tier_c; echo ""; fi
if [[ "$TIER" == "all" || "$TIER" == "D" ]]; then tier_d; echo ""; fi

if [[ -n "$SUMMARY_FILE" ]]; then
	mkdir -p "$(dirname "$SUMMARY_FILE")"
	cat > "$SUMMARY_FILE" <<EOF
{
  "tier": "$(json_escape "$TIER")",
  "strict": $STRICT,
  "failures": $failures,
  "records": [${STATUS_RECORDS}]
}
EOF
	note "wrote summary: $SUMMARY_FILE"
fi

if [[ "$failures" -ne 0 ]]; then
	echo "=== Renderer tier resolver: FAILED ($failures issue(s)) ===" >&2
	exit 1
fi

echo "=== Renderer tier resolver: complete ==="
