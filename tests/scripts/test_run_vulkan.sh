#!/usr/bin/env bash
# Regression tests for scripts/run_vulkan.sh launcher behavior.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
RUN_VULKAN_SRC="$PROJECT_ROOT/scripts/run_vulkan.sh"

TMP_ROOT="$(mktemp -d)"
trap 'rm -rf "$TMP_ROOT"' EXIT

FAKE_BIN_DIR="$TMP_ROOT/fake-bin"
mkdir -p "$FAKE_BIN_DIR"

write_fake_uname() {
  local arch="$1"
  cat > "$FAKE_BIN_DIR/uname" <<EOF
#!/usr/bin/env bash
echo "$arch"
EOF
  chmod +x "$FAKE_BIN_DIR/uname"
}

write_capture_engine() {
  local path="$1"
  local output_file="$2"
  cat > "$path" <<EOF
#!/usr/bin/env bash
echo "\$0" > "$output_file"
printf "%s\n" "\$*" >> "$output_file"
EOF
  chmod +x "$path"
}

assert_eq() {
  local expected="$1"
  local actual="$2"
  local label="$3"
  if [ "$expected" != "$actual" ]; then
    echo "FAIL: $label"
    echo "  expected: $expected"
    echo "  actual:   $actual"
    exit 1
  fi
}

# Case 1: when invoked from scripts/, launcher must execute ../release/idtech3.
CASE1_ROOT="$TMP_ROOT/case1"
mkdir -p "$CASE1_ROOT/project/scripts" "$CASE1_ROOT/project/release"
cp "$RUN_VULKAN_SRC" "$CASE1_ROOT/project/scripts/run_vulkan.sh"
chmod +x "$CASE1_ROOT/project/scripts/run_vulkan.sh"

CASE1_OUT="$CASE1_ROOT/invocation.txt"
CASE1_ENGINE="$CASE1_ROOT/project/release/idtech3"
write_capture_engine "$CASE1_ENGINE" "$CASE1_OUT"
write_fake_uname "x86_64"

PATH="$FAKE_BIN_DIR:$PATH" "$CASE1_ROOT/project/scripts/run_vulkan.sh" +set cl_renderer vulkan

CASE1_CALLED_PATH="$(sed -n '1p' "$CASE1_OUT")"
CASE1_CALLED_ARGS="$(sed -n '2p' "$CASE1_OUT")"
assert_eq "$CASE1_ENGINE" "$CASE1_CALLED_PATH" "scripts/ launch should use sibling release/idtech3"
assert_eq "+set cl_renderer vulkan" "$CASE1_CALLED_ARGS" "engine args should be forwarded untouched"

# Case 2: on aarch64, prefer idtech3.aarch64 when present.
CASE2_ROOT="$TMP_ROOT/case2"
mkdir -p "$CASE2_ROOT/project/scripts" "$CASE2_ROOT/project/release"
cp "$RUN_VULKAN_SRC" "$CASE2_ROOT/project/scripts/run_vulkan.sh"
chmod +x "$CASE2_ROOT/project/scripts/run_vulkan.sh"

CASE2_OUT="$CASE2_ROOT/invocation.txt"
CASE2_ENGINE_AARCH64="$CASE2_ROOT/project/release/idtech3.aarch64"
CASE2_ENGINE_FALLBACK="$CASE2_ROOT/project/release/idtech3"
write_capture_engine "$CASE2_ENGINE_AARCH64" "$CASE2_OUT"
write_capture_engine "$CASE2_ENGINE_FALLBACK" "$CASE2_OUT.fallback"
write_fake_uname "aarch64"

PATH="$FAKE_BIN_DIR:$PATH" "$CASE2_ROOT/project/scripts/run_vulkan.sh" +set r_mode -1

CASE2_CALLED_PATH="$(sed -n '1p' "$CASE2_OUT")"
CASE2_CALLED_ARGS="$(sed -n '2p' "$CASE2_OUT")"
assert_eq "$CASE2_ENGINE_AARCH64" "$CASE2_CALLED_PATH" "aarch64 launch should prefer idtech3.aarch64"
assert_eq "+set r_mode -1" "$CASE2_CALLED_ARGS" "aarch64 engine args should be forwarded untouched"

# Case 3: on aarch64, fall back to idtech3 when idtech3.aarch64 is not executable.
CASE3_ROOT="$TMP_ROOT/case3"
mkdir -p "$CASE3_ROOT/project/scripts" "$CASE3_ROOT/project/release"
cp "$RUN_VULKAN_SRC" "$CASE3_ROOT/project/scripts/run_vulkan.sh"
chmod +x "$CASE3_ROOT/project/scripts/run_vulkan.sh"

CASE3_OUT="$CASE3_ROOT/invocation.txt"
CASE3_ENGINE_AARCH64="$CASE3_ROOT/project/release/idtech3.aarch64"
CASE3_ENGINE_FALLBACK="$CASE3_ROOT/project/release/idtech3"
write_capture_engine "$CASE3_ENGINE_AARCH64" "$CASE3_OUT.unused"
chmod -x "$CASE3_ENGINE_AARCH64"
write_capture_engine "$CASE3_ENGINE_FALLBACK" "$CASE3_OUT"
write_fake_uname "aarch64"

PATH="$FAKE_BIN_DIR:$PATH" "$CASE3_ROOT/project/scripts/run_vulkan.sh" +set developer 1

CASE3_CALLED_PATH="$(sed -n '1p' "$CASE3_OUT")"
CASE3_CALLED_ARGS="$(sed -n '2p' "$CASE3_OUT")"
assert_eq "$CASE3_ENGINE_FALLBACK" "$CASE3_CALLED_PATH" "aarch64 launch should fall back to idtech3 when arch binary is not executable"
assert_eq "+set developer 1" "$CASE3_CALLED_ARGS" "fallback engine args should be forwarded untouched"

echo "PASS: test_run_vulkan"
