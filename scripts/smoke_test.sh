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
# shellcheck source=lib/release_bin.sh
source "$SCRIPT_DIR/lib/release_bin.sh"
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
bin_path() { release_bin_path "$RELEASE_DIR" "$1"; }

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

# --- Runtime generative hooks (FLUX / TRELLIS) ---
echo "Generative runtime checks:"
CLIENT_PATH="$(bin_path "idtech3")"
if [ -n "$CLIENT_PATH" ]; then
  if release_bin_has_text "$CLIENT_PATH" 'trellis_generate'; then
    pass "client exports trellis_generate (USE_TRELLIS)"
  else
    warn "trellis_generate not in client (USE_TRELLIS=OFF or stripped build)"
  fi
  if release_bin_has_text "$CLIENT_PATH" 'flux_generate'; then
    pass "client exports flux_generate (USE_FLUX)"
  else
    warn "flux_generate not in client (USE_FLUX=OFF or stripped build)"
  fi
  if release_bin_has_text "$CLIENT_PATH" 'spec_energy_generate'; then
    pass "client exports spec_energy_generate (USE_SPEC_ENERGY)"
  else
    warn "spec_energy_generate not in client (USE_SPEC_ENERGY=OFF or stripped build)"
  fi
  if release_bin_has_text "$CLIENT_PATH" 'beta_record|beta_test'; then
    pass "client exports beta trace commands"
  else
    warn "beta_record not in client (stripped build)"
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
if [ -f "$RELEASE_DIR/spec_energy_flux_generate.py" ]; then
  pass "spec_energy_flux_generate.py present in release"
  if head -1 "$RELEASE_DIR/spec_energy_flux_generate.py" | grep -q python; then
    pass "spec_energy_flux_generate.py looks like Python wrapper"
  fi
else
  warn "spec_energy_flux_generate.py missing (non-spec-energy build or old release copy)"
fi

if [ -x "$SCRIPT_DIR/spec_energy_runtime_check.sh" ]; then
  # Some CI jobs (eg ASAN) build via raw CMake and only copy binaries into release/,
  # so the Python wrapper may be intentionally absent. Treat that as a warning.
  if [ -f "$RELEASE_DIR/spec_energy_flux_generate.py" ]; then
    if "$SCRIPT_DIR/spec_energy_runtime_check.sh" "$RELEASE_DIR"; then
      pass "spec_energy_runtime_check.sh"
    else
      fail "spec_energy_runtime_check.sh"
    fi
  else
    warn "spec_energy_runtime_check skipped (release/spec_energy_flux_generate.py missing)"
  fi
fi

echo ""

# --- Q3 / OpenArena example cfgs in release ---
if [ -f "$RELEASE_DIR/examples/q3_vulkan_compat.cfg" ] && [ -f "$RELEASE_DIR/examples/q3_classic_mod.cfg" ]; then
  pass "release/examples Q3/OA cfgs present"
else
  warn "release/examples Q3/OA cfgs missing (re-run compile_engine.sh vulkan)"
fi

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
