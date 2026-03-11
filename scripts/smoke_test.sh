#!/usr/bin/env bash
set -euo pipefail

# Smoke test for id Tech 3 engine binaries.
# Validates that compiled binaries are functional ELFs/executables
# and that the dedicated server can initialize and exit cleanly.
#
# Usage: ./scripts/smoke_test.sh [release_dir]

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ -f "$SCRIPT_DIR/CMakeLists.txt" ]; then
  PROJECT_ROOT="$SCRIPT_DIR"
elif [ -f "$SCRIPT_DIR/../CMakeLists.txt" ]; then
  PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
else
  echo "Error: Could not find CMakeLists.txt" >&2
  exit 1
fi

RELEASE_DIR="${1:-$PROJECT_ROOT/release}"
PASS=0
FAIL=0
WARN=0

pass() { PASS=$((PASS + 1)); echo "  ✓ $1"; }
fail() { FAIL=$((FAIL + 1)); echo "  ✗ $1" >&2; }
warn() { WARN=$((WARN + 1)); echo "  ⚠ $1"; }

echo "=== id Tech 3 Smoke Test ==="
echo "Release dir: $RELEASE_DIR"
echo ""

# --- Binary existence checks ---
# Resolve binary path (handle Windows .exe and arch suffixes: .x64, .x86_64, .aarch64)
bin_path() {
  local bin="$1"
  local base="$RELEASE_DIR/$bin"
  # Try: idtech3, idtech3.exe, idtech3.x64, idtech3.x64.exe, idtech3.x86_64, idtech3.aarch64
  for candidate in "$base" "$base.exe" "$base.x64" "$base.x64.exe" "$base.x86_64" "$base.x86_64.exe" "$base.aarch64"; do
    if [ -f "$candidate" ]; then
      echo "$candidate"
      return
    fi
  done
  echo ""
}

echo "Binary checks:"
for bin in idtech3 idtech3_server; do
  path="$(bin_path "$bin")"
  if [ -n "$path" ]; then
    pass "$bin exists"
  else
    fail "$bin not found"
  fi
done

for lib in idtech3_vulkan.so idtech3_opengl.so idtech3_vulkan.dylib idtech3_opengl.dylib idtech3_vulkan.dll idtech3_opengl.dll; do
  if [ -f "$RELEASE_DIR/$lib" ]; then
    pass "$lib exists"
  fi
done
if [ ! -f "$RELEASE_DIR/idtech3_vulkan.so" ] && [ ! -f "$RELEASE_DIR/idtech3_vulkan.dylib" ] && [ ! -f "$RELEASE_DIR/idtech3_vulkan.dll" ]; then
  warn "renderer libs not found (may be statically linked)"
fi

echo ""

# --- Binary format checks ---
echo "Format checks:"
for bin in idtech3 idtech3_server; do
  path="$(bin_path "$bin")"
  if [ -n "$path" ]; then
    filetype="$(file -b "$path" 2>/dev/null || echo "unknown")"
    if echo "$filetype" | grep -q "ELF\|Mach-O\|PE32"; then
      pass "$bin is a valid executable ($filetype)"
    else
      fail "$bin has unexpected format: $filetype"
    fi
  fi
done

echo ""

# --- Dedicated server startup test ---
echo "Server startup test:"
SERVER_PATH="$(bin_path "idtech3_server")"
if [ -n "$SERVER_PATH" ]; then
  output="$(timeout 5 "$SERVER_PATH" +set dedicated 1 +set com_hunkMegs 64 +quit 2>&1 || true)"

  if echo "$output" | grep -q "id Tech 3"; then
    pass "Server identifies as id Tech 3"
  else
    fail "Server did not identify correctly"
  fi

  if echo "$output" | grep -q "FS_Startup\|FS_Init"; then
    pass "Filesystem initialized"
  else
    fail "Filesystem did not initialize"
  fi

  if echo "$output" | grep -qi "segfault\|SIGSEGV\|core dump\|abort"; then
    fail "Server crashed during startup"
  else
    pass "No crashes detected"
  fi
else
  fail "idtech3_server not found, cannot run startup test"
fi


echo ""

# --- Shader validation ---
echo "Shader checks:"
if command -v glslangValidator &>/dev/null; then
  shader_errors=0
  shader_dir="$PROJECT_ROOT/src/renderers/vulkan/shaders/glsl"

  for shader in "$shader_dir"/*.vert "$shader_dir"/*.frag; do
    [ -f "$shader" ] || continue
    if ! glslangValidator -V "$shader" -o /dev/null 2>/dev/null; then
      fail "Shader validation failed: $(basename "$shader")"
      shader_errors=$((shader_errors + 1))
    fi
  done

  if [ "$shader_errors" -eq 0 ]; then
    pass "All standalone shaders pass validation"
  fi
else
  warn "glslangValidator not found, skipping shader validation"
fi

echo ""

# --- Summary ---
echo "=== Results ==="
echo "  Passed: $PASS"
echo "  Failed: $FAIL"
echo "  Warnings: $WARN"

if [ "$FAIL" -gt 0 ]; then
  echo ""
  echo "SMOKE TEST FAILED"
  exit 1
else
  echo ""
  echo "SMOKE TEST PASSED"
  exit 0
fi
