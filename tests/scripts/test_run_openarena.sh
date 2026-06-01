#!/usr/bin/env bash
# Regression tests for scripts/run_openarena.sh (stub client, no game data).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
LAUNCHER="${1:-$PROJECT_ROOT/scripts/run_openarena.sh}"

if [ ! -f "$LAUNCHER" ]; then
	echo "Error: launcher not found: $LAUNCHER" >&2
	exit 2
fi

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

assert_contains() {
	local haystack="$1"
	local needle="$2"
	local context="$3"
	if [[ "$haystack" != *"$needle"* ]]; then
		fail "$context: expected '$needle' in output"
	fi
}

TMP_ROOT="$(mktemp -d)"
trap 'rm -rf "$TMP_ROOT"' EXIT

mkdir -p "$TMP_ROOT/release" "$TMP_ROOT/examples" "$TMP_ROOT/oa_base"
cat > "$TMP_ROOT/release/idtech3" <<'EOF'
#!/usr/bin/env bash
for arg in "$@"; do
	echo "ARG:$arg"
done
EOF
chmod +x "$TMP_ROOT/release/idtech3"
cp "$PROJECT_ROOT/examples/q3_vulkan_compat.cfg" "$TMP_ROOT/examples/" 2>/dev/null || \
	echo 'seta cl_renderer "vulkan"' > "$TMP_ROOT/examples/q3_vulkan_compat.cfg"
cp "$PROJECT_ROOT/examples/q3_classic_mod.cfg" "$TMP_ROOT/examples/" 2>/dev/null || true

# Patch launcher paths: use RELEASE_DIR and PROJECT_ROOT from env
export RELEASE_DIR="$TMP_ROOT/release"
export OA_BASE="$TMP_ROOT/oa_base"

out_default="$(bash "$LAUNCHER" 2>&1)"
assert_contains "$out_default" "ARG:+set" "default launch"
assert_contains "$out_default" "ARG:cl_renderer" "default vulkan"
assert_contains "$out_default" "ARG:vulkan" "default vulkan"

out_classic="$(CLASSIC_MOD=1 bash "$LAUNCHER" 2>&1)"
assert_contains "$out_classic" "ARG:r_classicMod" "classic mod cvar"
assert_contains "$out_classic" "ARG:1" "classic mod value"
assert_contains "$out_classic" "ARG:r_forwardPlus" "classic forward+ off"
assert_contains "$out_classic" "ARG:0" "classic zero cvars"

# OA_BASE should copy cfg and exec
if [ -f "$TMP_ROOT/oa_base/q3_vulkan_compat.cfg" ]; then
	:
else
	# Launcher copies from PROJECT_ROOT/examples — set PROJECT_ROOT via script location
	PROJECT_ROOT="$PROJECT_ROOT" OA_BASE="$TMP_ROOT/oa_base" RELEASE_DIR="$TMP_ROOT/release" \
		bash "$LAUNCHER" >/dev/null 2>&1 || true
fi
if [ ! -f "$TMP_ROOT/oa_base/q3_vulkan_compat.cfg" ]; then
	fail "OA_BASE copy did not create q3_vulkan_compat.cfg"
fi

out_oa="$(OA_BASE="$TMP_ROOT/oa_base" bash "$LAUNCHER" 2>&1)"
assert_contains "$out_oa" "ARG:+exec" "OA_BASE exec"
assert_contains "$out_oa" "ARG:q3_vulkan_compat" "OA_BASE exec cfg"

mkdir -p "$TMP_ROOT/oa_openarena/OpenArena"
out_auto="$(OA_BASE="$TMP_ROOT/oa_openarena/OpenArena" AUTO_CLASSIC=1 bash "$LAUNCHER" 2>&1)"
assert_contains "$out_auto" "AUTO_CLASSIC" "auto classic log"
assert_contains "$out_auto" "ARG:r_classicMod" "auto classic cvar"

echo "PASS: test_run_openarena"
