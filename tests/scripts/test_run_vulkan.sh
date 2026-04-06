#!/usr/bin/env bash
# Regression tests for scripts/run_vulkan.sh launcher behavior.
set -euo pipefail

SCRIPT_UNDER_TEST="${1:-}"
if [ -z "$SCRIPT_UNDER_TEST" ] || [ ! -f "$SCRIPT_UNDER_TEST" ]; then
  echo "Usage: $0 /absolute/or/relative/path/to/scripts/run_vulkan.sh" >&2
  exit 1
fi

SCRIPT_UNDER_TEST="$(cd "$(dirname "$SCRIPT_UNDER_TEST")" && pwd)/$(basename "$SCRIPT_UNDER_TEST")"

TMP_ROOT="$(mktemp -d)"
trap 'rm -rf "$TMP_ROOT"' EXIT

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

assert_contains() {
  local haystack="$1"
  local needle="$2"
  local msg="$3"
  if [[ "$haystack" != *"$needle"* ]]; then
    fail "$msg (missing: $needle)"
  fi
}

assert_not_contains() {
  local haystack="$1"
  local needle="$2"
  local msg="$3"
  if [[ "$haystack" == *"$needle"* ]]; then
    fail "$msg (unexpected: $needle)"
  fi
}

make_engine_stub() {
  local path="$1"
  local marker="$2"
  cat >"$path" <<EOF_STUB
#!/usr/bin/env bash
echo "MARKER:${marker}"
echo "ARGS:\$*"
EOF_STUB
  chmod +x "$path"
}

make_uname_stub() {
  local path="$1"
  local arch="$2"
  cat >"$path" <<EOF_UNAME
#!/usr/bin/env bash
if [ "\${1:-}" = "-m" ]; then
  echo "${arch}"
else
  /usr/bin/uname "\$@"
fi
EOF_UNAME
  chmod +x "$path"
}

new_repo_layout() {
  local name="$1"
  local root="$TMP_ROOT/$name"
  mkdir -p "$root/repo/scripts" "$root/repo/release" "$root/fakebin"
  cp "$SCRIPT_UNDER_TEST" "$root/repo/scripts/run_vulkan.sh"
  chmod +x "$root/repo/scripts/run_vulkan.sh"
  echo "$root"
}

# 1) When run from scripts/, prefer sibling release/idtech3.
layout1="$(new_repo_layout case1)"
make_engine_stub "$layout1/repo/release/idtech3" "release-default"
make_engine_stub "$layout1/repo/scripts/idtech3" "scripts-local"
out1="$("$layout1/repo/scripts/run_vulkan.sh" +set fs_game unwaking +set cl_renderer vulkan)"
assert_contains "$out1" "MARKER:release-default" "scripts launcher should prefer sibling release binary"
assert_not_contains "$out1" "MARKER:scripts-local" "scripts-local binary should not be used when release binary exists"
assert_contains "$out1" "ARGS:+set fs_game unwaking +set cl_renderer vulkan" "arguments should be forwarded unchanged"

# 2) On arm64/aarch64, choose idtech3.aarch64 when available.
layout2="$(new_repo_layout case2)"
make_engine_stub "$layout2/repo/release/idtech3" "release-default"
make_engine_stub "$layout2/repo/release/idtech3.aarch64" "release-arm64"
make_uname_stub "$layout2/fakebin/uname" "arm64"
out2="$(PATH="$layout2/fakebin:$PATH" "$layout2/repo/scripts/run_vulkan.sh" +set cl_renderer vulkan)"
assert_contains "$out2" "MARKER:release-arm64" "arm64 should select idtech3.aarch64 when present"

# 3) On arm64/aarch64, fall back to idtech3 when arch-specific binary is missing.
layout3="$(new_repo_layout case3)"
make_engine_stub "$layout3/repo/release/idtech3" "release-default"
make_uname_stub "$layout3/fakebin/uname" "aarch64"
out3="$(PATH="$layout3/fakebin:$PATH" "$layout3/repo/scripts/run_vulkan.sh" +set r_fullscreen 0)"
assert_contains "$out3" "MARKER:release-default" "missing idtech3.aarch64 should fall back to idtech3"

echo "PASS: test_run_vulkan.sh"
