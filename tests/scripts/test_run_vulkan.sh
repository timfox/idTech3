#!/usr/bin/env bash
# Regression tests for scripts/run_vulkan.sh path and arch selection.
set -euo pipefail

PROJECT_ROOT="${1:-}"
if [[ -z "$PROJECT_ROOT" ]]; then
  echo "Usage: $0 <project-root>" >&2
  exit 1
fi

SOURCE_SCRIPT="$PROJECT_ROOT/scripts/run_vulkan.sh"
if [[ ! -f "$SOURCE_SCRIPT" ]]; then
  echo "Missing source script: $SOURCE_SCRIPT" >&2
  exit 1
fi

TMP_ROOT="$(mktemp -d)"
trap 'rm -rf "$TMP_ROOT"' EXIT

make_engine() {
  local path="$1"
  local marker="$2"
  cat > "$path" <<EOF
#!/usr/bin/env bash
echo "MARKER=$marker"
echo "ENGINE_PATH=\$0"
echo "ARGC=\$#"
EOF
  chmod +x "$path"
}

assert_contains() {
  local haystack="$1"
  local needle="$2"
  local name="$3"
  if [[ "$haystack" != *"$needle"* ]]; then
    echo "FAIL: $name (missing '$needle')" >&2
    echo "Output was:" >&2
    echo "$haystack" >&2
    exit 1
  fi
}

# Case 1: when run from repo scripts/, it should prefer sibling release/.
CASE1="$TMP_ROOT/case1"
mkdir -p "$CASE1/repo/scripts" "$CASE1/repo/release"
cp "$SOURCE_SCRIPT" "$CASE1/repo/scripts/run_vulkan.sh"
chmod +x "$CASE1/repo/scripts/run_vulkan.sh"
make_engine "$CASE1/repo/release/idtech3" "release"
make_engine "$CASE1/repo/scripts/idtech3" "scripts"
OUT1="$("$CASE1/repo/scripts/run_vulkan.sh" "+set" "cl_renderer" "vulkan")"
assert_contains "$OUT1" "MARKER=release" "case1 prefers release engine"
assert_contains "$OUT1" "ARGC=3" "case1 forwards args"

# Case 2: when copied into release/, it should run local engine.
CASE2="$TMP_ROOT/case2"
mkdir -p "$CASE2/release"
cp "$SOURCE_SCRIPT" "$CASE2/release/run_vulkan.sh"
chmod +x "$CASE2/release/run_vulkan.sh"
make_engine "$CASE2/release/idtech3" "local"
OUT2="$("$CASE2/release/run_vulkan.sh")"
assert_contains "$OUT2" "MARKER=local" "case2 falls back to local release dir"

# Case 3: on aarch64, prefer idtech3.aarch64, then fallback to idtech3 if not executable.
CASE3="$TMP_ROOT/case3"
mkdir -p "$CASE3/repo/scripts" "$CASE3/repo/release" "$CASE3/bin"
cp "$SOURCE_SCRIPT" "$CASE3/repo/scripts/run_vulkan.sh"
chmod +x "$CASE3/repo/scripts/run_vulkan.sh"
cat > "$CASE3/bin/uname" <<'EOF'
#!/usr/bin/env bash
echo "aarch64"
EOF
chmod +x "$CASE3/bin/uname"
make_engine "$CASE3/repo/release/idtech3.aarch64" "arm64"
make_engine "$CASE3/repo/release/idtech3" "generic"

OUT3A="$(PATH="$CASE3/bin:$PATH" "$CASE3/repo/scripts/run_vulkan.sh")"
assert_contains "$OUT3A" "MARKER=arm64" "case3 prefers aarch64 binary"

chmod -x "$CASE3/repo/release/idtech3.aarch64"
OUT3B="$(PATH="$CASE3/bin:$PATH" "$CASE3/repo/scripts/run_vulkan.sh")"
assert_contains "$OUT3B" "MARKER=generic" "case3 falls back to generic binary"

echo "PASS: test_run_vulkan"
