#!/usr/bin/env bash
# Regression test for scripts/run_vulkan.sh engine path selection.
set -euo pipefail

SCRIPT_UNDER_TEST="${1:-}"
if [ -z "$SCRIPT_UNDER_TEST" ] || [ ! -f "$SCRIPT_UNDER_TEST" ]; then
  echo "Usage: $0 /absolute/or/relative/path/to/scripts/run_vulkan.sh"
  exit 1
fi

ROOT_DIR="$(cd "$(dirname "$SCRIPT_UNDER_TEST")/.." && pwd)"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

expected_engine_name() {
  case "$(uname -m)" in
    aarch64|arm64) echo "idtech3.aarch64" ;;
    x86_64) echo "idtech3" ;;
    *) echo "idtech3" ;;
  esac
}

create_fake_engine() {
  local path="$1"
  cat >"$path" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
echo "ENGINE_PATH=$0"
echo "ENGINE_NAME=$(basename "$0")"
echo "ENGINE_DIR=$(cd "$(dirname "$0")" && pwd)"
echo "ENGINE_ARGS=$*"
EOF
  chmod +x "$path"
}

assert_contains() {
  local haystack="$1"
  local needle="$2"
  if [[ "$haystack" != *"$needle"* ]]; then
    echo "Assertion failed: expected output to contain: $needle"
    echo "---- output ----"
    echo "$haystack"
    echo "----------------"
    exit 1
  fi
}

run_repo_layout_case() {
  local case_dir="$TMP_DIR/repo_layout"
  local scripts_dir="$case_dir/scripts"
  local release_dir="$case_dir/release"
  local output
  local expected_name

  mkdir -p "$scripts_dir" "$release_dir"
  cp "$SCRIPT_UNDER_TEST" "$scripts_dir/run_vulkan.sh"
  chmod +x "$scripts_dir/run_vulkan.sh"

  create_fake_engine "$release_dir/idtech3"
  create_fake_engine "$release_dir/idtech3.aarch64"

  output="$("$scripts_dir/run_vulkan.sh" +set cl_renderer vulkan)"
  expected_name="$(expected_engine_name)"

  assert_contains "$output" "ENGINE_DIR=$release_dir"
  assert_contains "$output" "ENGINE_NAME=$expected_name"
  assert_contains "$output" "ENGINE_ARGS=+set cl_renderer vulkan"
}

run_copied_layout_case() {
  local case_dir="$TMP_DIR/copied_layout"
  local output
  local expected_name

  mkdir -p "$case_dir"
  cp "$SCRIPT_UNDER_TEST" "$case_dir/run_vulkan.sh"
  chmod +x "$case_dir/run_vulkan.sh"

  create_fake_engine "$case_dir/idtech3"
  create_fake_engine "$case_dir/idtech3.aarch64"

  output="$("$case_dir/run_vulkan.sh" +set cl_renderer vulkan)"
  expected_name="$(expected_engine_name)"

  assert_contains "$output" "ENGINE_DIR=$case_dir"
  assert_contains "$output" "ENGINE_NAME=$expected_name"
  assert_contains "$output" "ENGINE_ARGS=+set cl_renderer vulkan"
}

run_repo_layout_case
run_copied_layout_case

echo "PASS: run_vulkan path resolution"
