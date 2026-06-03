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
  # Try: plain name, .exe, arch suffixes (Windows/Linux), macOS .app bundle executable
  for candidate in \
    "$base" "$base.exe" \
    "$base.x64" "$base.x64.exe" "$base.x86_64" "$base.x86_64.exe" \
    "$base.aarch64" "$base.arm" "$base.armv7l" \
    "$base.aarch64.app/Contents/MacOS/$bin.aarch64" \
    "$base.aarch64.app/Contents/MacOS/$bin" \
    "$base.arm.app/Contents/MacOS/$bin.arm" \
    "$base.arm.app/Contents/MacOS/$bin" \
    "$base.app/Contents/MacOS/$bin"; do
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

# Windows OpenAL Soft bundle (CI / release layout): router OpenAL32.dll + soft_oal.dll next to the client.
if [ -f "$RELEASE_DIR/OpenAL32.dll" ] || [ -f "$RELEASE_DIR/soft_oal.dll" ]; then
  if [ -f "$RELEASE_DIR/OpenAL32.dll" ] && [ -f "$RELEASE_DIR/soft_oal.dll" ]; then
    pass "OpenAL Soft bundle present (OpenAL32.dll + soft_oal.dll)"
  else
    fail "Incomplete OpenAL bundle in $RELEASE_DIR (need both OpenAL32.dll and soft_oal.dll)"
  fi
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
  # Parallel ctest (-j) can starve the server + duplicate glslang work; allow enough wall time.
  output="$(timeout 25 "$SERVER_PATH" +set dedicated 1 +set com_hunkMegs 64 +quit 2>&1 || true)"

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

# --- FreeUSD (default USE_FREEUSD=ON) ---
echo "FreeUSD checks:"
VK_SO=""
for candidate in \
  "$RELEASE_DIR/idtech3_vulkan.so" \
  "$RELEASE_DIR/idtech3_vulkan_x86_64.so"; do
  if [ -f "$candidate" ]; then
    VK_SO="$candidate"
    break
  fi
done
if [ -n "$VK_SO" ]; then
  if grep -Fq 'R_RegisterFreeusdMesh' < <(nm -D "$VK_SO" 2>/dev/null); then
    pass "Vulkan renderer exports FreeUSD mesh loader"
  else
    warn "Vulkan renderer missing R_RegisterFreeusdMesh (built with USE_FREEUSD=OFF?)"
  fi
else
  warn "idtech3_vulkan.so not found for FreeUSD check"
fi

echo ""

# --- Runtime generative hooks (FLUX / TRELLIS) ---
echo "Generative runtime checks:"
CLIENT_PATH="$(bin_path "idtech3")"
if [ -n "$CLIENT_PATH" ]; then
  if grep -q trellis_generate < <(strings "$CLIENT_PATH" 2>/dev/null); then
    pass "client exports trellis_generate (USE_TRELLIS)"
  else
    warn "trellis_generate not in client (USE_TRELLIS=OFF or stripped build)"
  fi
  if grep -q flux_generate < <(strings "$CLIENT_PATH" 2>/dev/null); then
    pass "client exports flux_generate (USE_FLUX)"
  else
    warn "flux_generate not in client (USE_FLUX=OFF or stripped build)"
  fi
else
  warn "idtech3 not found, skipping generative symbol checks"
fi
if [ -f "$RELEASE_DIR/trellis_image_to_glb.py" ]; then
  pass "trellis_image_to_glb.py present in release"
  if [ -x "$RELEASE_DIR/trellis_image_to_glb.py" ] || head -1 "$RELEASE_DIR/trellis_image_to_glb.py" | grep -q python; then
    pass "trellis_image_to_glb.py looks executable"
  fi
else
  warn "trellis_image_to_glb.py missing (non-TRELLIS build or old release copy)"
fi

echo ""

# --- Q3 / OpenArena QVM compatibility (static; no game data required) ---
echo "Q3 / OpenArena compatibility:"
if [ -x "$PROJECT_ROOT/scripts/q3_openarena_compat_check.sh" ]; then
  if "$PROJECT_ROOT/scripts/q3_openarena_compat_check.sh" "$RELEASE_DIR"; then
    pass "q3_openarena_compat_check.sh"
  else
    fail "q3_openarena_compat_check.sh"
  fi
else
  warn "q3_openarena_compat_check.sh not executable"
fi

echo ""

# --- Shader validation ---
echo "Shader checks:"
if command -v glslangValidator &>/dev/null; then
  shader_errors=0
  shader_dir="$PROJECT_ROOT/src/renderers/vulkan/shaders/glsl"
  shader_count=0

  while IFS= read -r -d '' shader; do
    shader_count=$((shader_count + 1))
    rel="${shader#"$PROJECT_ROOT"/}"
    if ! err="$(glslangValidator -V "$shader" -o /dev/null 2>&1)"; then
      echo "$err" >&2
      fail "Shader validation failed: $rel"
      shader_errors=$((shader_errors + 1))
    fi
  done < <(find "$shader_dir" -type f \( -name '*.vert' -o -name '*.frag' -o -name '*.geom' -o -name '*.comp' \) -print0 | sort -z)

  if [ "$shader_count" -eq 0 ]; then
    fail "No GLSL stage files found under $shader_dir"
  elif [ "$shader_errors" -eq 0 ]; then
    pass "All $shader_count GLSL stage files pass validation (recursive)"
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
