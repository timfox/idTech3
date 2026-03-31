#!/usr/bin/env bash
# Unit-style test for scripts/run_vulkan.sh launcher behavior.
set -euo pipefail

PROJECT_ROOT="${1:-}"
if [ -z "$PROJECT_ROOT" ] || [ ! -d "$PROJECT_ROOT" ]; then
  echo "Usage: $0 <project-root>"
  exit 1
fi

LAUNCHER_SOURCE="$PROJECT_ROOT/scripts/run_vulkan.sh"
if [ ! -f "$LAUNCHER_SOURCE" ]; then
  echo "Launcher not found: $LAUNCHER_SOURCE"
  exit 1
fi

TMP_ROOT="$(mktemp -d)"
trap 'rm -rf "$TMP_ROOT"' EXIT

FAKE_BIN="$TMP_ROOT/fake-bin"
mkdir -p "$FAKE_BIN"
cat > "$FAKE_BIN/uname" <<'EOF'
#!/usr/bin/env bash
if [ "${1:-}" = "-m" ]; then
  printf '%s\n' "${TEST_UNAME_M:-x86_64}"
  exit 0
fi
exec /usr/bin/uname "$@"
EOF
chmod +x "$FAKE_BIN/uname"

make_engine_stub() {
  local engine_path="$1"
  cat > "$engine_path" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
printf 'ENGINE=%s\n' "$0"
printf 'LD_LIBRARY_PATH=%s\n' "${LD_LIBRARY_PATH-}"
printf 'ARGS='
for arg in "$@"; do
  printf '<%s>' "$arg"
done
printf '\n'
EOF
  chmod +x "$engine_path"
}

assert_contains() {
  local haystack="$1"
  local needle="$2"
  local msg="$3"
  if [[ "$haystack" != *"$needle"* ]]; then
    echo "ASSERTION FAILED: $msg"
    echo "Expected to find: $needle"
    echo "Got:"
    echo "$haystack"
    exit 1
  fi
}

expected_sdl_path() {
  local home_dir="$1"
  if [ -f "/usr/local/lib/libSDL2.so" ] || [ -f "/usr/local/lib/aarch64-linux-gnu/libSDL2.so" ]; then
    printf '/usr/local/lib'
    return
  fi
  if [ -f "$home_dir/sdl2-vulkan-install/lib/libSDL2.so" ]; then
    printf '%s' "$home_dir/sdl2-vulkan-install/lib"
    return
  fi
  printf ''
}

run_launcher() {
  local launcher="$1"
  local uname_m="$2"
  local home_dir="$3"
  shift 3

  env -u LD_LIBRARY_PATH \
    PATH="$FAKE_BIN:$PATH" \
    TEST_UNAME_M="$uname_m" \
    HOME="$home_dir" \
    "$launcher" "$@"
}

# Case 1: when launcher is in scripts/, resolve engine in ../release and pass args.
CASE1_ROOT="$TMP_ROOT/case1"
mkdir -p "$CASE1_ROOT/scripts" "$CASE1_ROOT/release" "$CASE1_ROOT/home/sdl2-vulkan-install/lib"
cp "$LAUNCHER_SOURCE" "$CASE1_ROOT/scripts/run_vulkan.sh"
chmod +x "$CASE1_ROOT/scripts/run_vulkan.sh"
touch "$CASE1_ROOT/home/sdl2-vulkan-install/lib/libSDL2.so"
make_engine_stub "$CASE1_ROOT/release/idtech3"

case1_output="$(run_launcher "$CASE1_ROOT/scripts/run_vulkan.sh" "x86_64" "$CASE1_ROOT/home" +set cl_renderer vulkan)"
assert_contains "$case1_output" "ENGINE=$CASE1_ROOT/release/idtech3" "scripts launcher should use ../release/idtech3"
assert_contains "$case1_output" "ARGS=<+set><cl_renderer><vulkan>" "engine args should be forwarded unchanged"
case1_expected_sdl="$(expected_sdl_path "$CASE1_ROOT/home")"
assert_contains "$case1_output" "LD_LIBRARY_PATH=$case1_expected_sdl" "LD_LIBRARY_PATH should follow SDL preference rules"

# Case 2: aarch64 should prefer idtech3.aarch64 when executable exists.
make_engine_stub "$CASE1_ROOT/release/idtech3.aarch64"
case2_output="$(run_launcher "$CASE1_ROOT/scripts/run_vulkan.sh" "aarch64" "$CASE1_ROOT/home" +set cl_renderer vulkan)"
assert_contains "$case2_output" "ENGINE=$CASE1_ROOT/release/idtech3.aarch64" "aarch64 launcher should prefer idtech3.aarch64"

# Case 3: aarch64 fallback to idtech3 when idtech3.aarch64 is missing/non-executable.
chmod -x "$CASE1_ROOT/release/idtech3.aarch64"
case3_output="$(run_launcher "$CASE1_ROOT/scripts/run_vulkan.sh" "aarch64" "$CASE1_ROOT/home" +set cl_renderer vulkan)"
assert_contains "$case3_output" "ENGINE=$CASE1_ROOT/release/idtech3" "launcher should fall back to idtech3 if arch binary is unusable"

# Case 4: if ../release does not exist, launcher should fall back to its own directory.
CASE4_ROOT="$TMP_ROOT/case4"
mkdir -p "$CASE4_ROOT/standalone" "$CASE4_ROOT/home/sdl2-vulkan-install/lib"
cp "$LAUNCHER_SOURCE" "$CASE4_ROOT/standalone/run_vulkan.sh"
chmod +x "$CASE4_ROOT/standalone/run_vulkan.sh"
touch "$CASE4_ROOT/home/sdl2-vulkan-install/lib/libSDL2.so"
make_engine_stub "$CASE4_ROOT/standalone/idtech3"

case4_output="$(run_launcher "$CASE4_ROOT/standalone/run_vulkan.sh" "x86_64" "$CASE4_ROOT/home" +set cl_renderer vulkan)"
assert_contains "$case4_output" "ENGINE=$CASE4_ROOT/standalone/idtech3" "standalone launcher should use script directory engine"
case4_expected_sdl="$(expected_sdl_path "$CASE4_ROOT/home")"
assert_contains "$case4_output" "LD_LIBRARY_PATH=$case4_expected_sdl" "standalone LD_LIBRARY_PATH should follow SDL preference rules"

echo "PASS: run_vulkan launcher behavior"
