#!/usr/bin/env bash
# Regression test for scripts/run_vulkan.sh path and arch resolution.
set -euo pipefail

SOURCE_RUNNER="${1:-}"
if [ -z "$SOURCE_RUNNER" ]; then
  SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  SOURCE_RUNNER="$(cd "$SCRIPT_DIR/../.." && pwd)/scripts/run_vulkan.sh"
fi

if [ ! -f "$SOURCE_RUNNER" ]; then
  echo "FAIL: run_vulkan.sh not found: $SOURCE_RUNNER"
  exit 1
fi

REAL_UNAME="$(command -v uname)"

fail() {
  echo "FAIL: $1"
  exit 1
}

assert_contains() {
  local haystack="$1"
  local needle="$2"
  local label="$3"

  case "$haystack" in
    *"$needle"*) ;;
    *)
      echo "ASSERT FAILED: $label"
      echo "Expected to find: $needle"
      echo "Actual output:"
      printf '%s\n' "$haystack"
      exit 1
      ;;
  esac
}

make_fake_engine() {
  local engine_path="$1"

  cat > "$engine_path" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
echo "engine=$0"
echo "argc=$#"
i=1
for arg in "$@"; do
  echo "arg${i}=${arg}"
  i=$((i + 1))
done
EOF
  chmod +x "$engine_path"
}

make_fake_uname() {
  local bin_dir="$1"

  cat > "$bin_dir/uname" <<EOF
#!/usr/bin/env bash
set -euo pipefail
if [ "\${1:-}" = "-m" ]; then
  echo "\${TEST_ARCH:-x86_64}"
else
  exec "$REAL_UNAME" "\$@"
fi
EOF
  chmod +x "$bin_dir/uname"
}

run_case_repo_layout_x64() {
  local tmp="$1"
  local runner="$tmp/project/scripts/run_vulkan.sh"
  local release="$tmp/project/release"
  local bin="$tmp/bin"
  local out

  mkdir -p "$tmp/project/scripts" "$release" "$bin" "$tmp/home"
  cp "$SOURCE_RUNNER" "$runner"
  chmod +x "$runner"
  make_fake_engine "$release/idtech3"
  make_fake_uname "$bin"

  out="$(PATH="$bin:$PATH" HOME="$tmp/home" TEST_ARCH="x86_64" "$runner" "+set" "fs_game" "arg with space")"
  assert_contains "$out" "engine=$release/idtech3" "repo layout uses release/idtech3"
  assert_contains "$out" "argc=3" "arguments count preserved"
  assert_contains "$out" "arg1=+set" "arg 1 forwarded"
  assert_contains "$out" "arg2=fs_game" "arg 2 forwarded"
  assert_contains "$out" "arg3=arg with space" "arg with space forwarded"
}

run_case_portable_layout() {
  local tmp="$1"
  local runner="$tmp/portable/run_vulkan.sh"
  local bin="$tmp/bin"
  local out

  mkdir -p "$tmp/portable" "$bin" "$tmp/home"
  cp "$SOURCE_RUNNER" "$runner"
  chmod +x "$runner"
  make_fake_engine "$tmp/portable/idtech3"
  make_fake_uname "$bin"

  out="$(PATH="$bin:$PATH" HOME="$tmp/home" TEST_ARCH="x86_64" "$runner" "demoarg")"
  assert_contains "$out" "engine=$tmp/portable/idtech3" "portable layout falls back to script dir"
  assert_contains "$out" "argc=1" "single argument preserved"
  assert_contains "$out" "arg1=demoarg" "portable argument forwarded"
}

run_case_aarch64_prefers_arch_binary() {
  local tmp="$1"
  local runner="$tmp/project/scripts/run_vulkan.sh"
  local release="$tmp/project/release"
  local bin="$tmp/bin"
  local out

  mkdir -p "$tmp/project/scripts" "$release" "$bin" "$tmp/home"
  cp "$SOURCE_RUNNER" "$runner"
  chmod +x "$runner"
  make_fake_engine "$release/idtech3"
  make_fake_engine "$release/idtech3.aarch64"
  make_fake_uname "$bin"

  out="$(PATH="$bin:$PATH" HOME="$tmp/home" TEST_ARCH="aarch64" "$runner")"
  assert_contains "$out" "engine=$release/idtech3.aarch64" "aarch64 prefers arch-specific binary"
}

run_case_aarch64_falls_back_to_generic() {
  local tmp="$1"
  local runner="$tmp/project/scripts/run_vulkan.sh"
  local release="$tmp/project/release"
  local bin="$tmp/bin"
  local out

  mkdir -p "$tmp/project/scripts" "$release" "$bin" "$tmp/home"
  cp "$SOURCE_RUNNER" "$runner"
  chmod +x "$runner"
  make_fake_engine "$release/idtech3"
  make_fake_uname "$bin"

  out="$(PATH="$bin:$PATH" HOME="$tmp/home" TEST_ARCH="aarch64" "$runner")"
  assert_contains "$out" "engine=$release/idtech3" "aarch64 falls back to idtech3"
}

main() {
  local tmp
  tmp="$(mktemp -d)"
  trap "rm -rf '$tmp'" EXIT

  run_case_repo_layout_x64 "$tmp/case1"
  run_case_portable_layout "$tmp/case2"
  run_case_aarch64_prefers_arch_binary "$tmp/case3"
  run_case_aarch64_falls_back_to_generic "$tmp/case4"

  echo "PASS: test_run_vulkan.sh"
}

main "$@"
