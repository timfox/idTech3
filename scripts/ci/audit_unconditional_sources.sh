#!/usr/bin/env bash
# Fail if new unconditional QCOMMON_SRCS / CLIENT_SRCS appends appear outside approved cmake modules.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

CM="${ROOT}/CMakeLists.txt"
FAIL=0

pass() { echo "  ok: $1"; }
fail() { echo "  FAIL: $1" >&2; FAIL=1; }

echo "=== audit_unconditional_sources ==="

# Allowed append locations (macros / includes handle extension sources).
APPROVED=(
  "cmake/IdTech3QcommonExtensions.cmake"
  "cmake/client/ClientExtensionSources.cmake"
  "cmake/renderers/VulkanExtensionSources.cmake"
)

# Patterns that must not appear as bare unconditional extension paths in CMakeLists.txt root.
FORBIDDEN_PATHS=(
  "src/extensions/research/"
  "src/extensions/generative/"
  "src/world/world_district.cpp"
  "src/world/world_open.cpp"
  "src/world/fog_biology.cpp"
  "src/world/genetic_gan.cpp"
  "src/qcommon/cluster_graph.cpp"
  "src/qcommon/cm_stream_merge.c"
)

for pat in "${FORBIDDEN_PATHS[@]}"; do
  if grep -q "list(APPEND QCOMMON_SRCS.*${pat}" "$CM" 2>/dev/null || \
     grep -q "list(APPEND CLIENT_SRCS.*${pat}" "$CM" 2>/dev/null; then
    fail "unconditional list(APPEND ...) for ${pat} in CMakeLists.txt — use cmake module macros"
  fi
done

# Duplicate vksplat guard (historical bug: double list(APPEND)).
if grep -q 'list(APPEND QCOMMON_SRCS.*vksplat_model.c' "$CM" 2>/dev/null; then
  count="$(grep -c 'list(APPEND QCOMMON_SRCS.*vksplat_model.c' "$CM" || true)"
  if [ "$count" -gt 1 ]; then
    fail "duplicate unconditional vksplat list(APPEND in CMakeLists.txt (${count})"
  fi
fi

for f in "${APPROVED[@]}"; do
  if [ ! -f "$ROOT/$f" ]; then
    fail "missing approved cmake module: $f"
  else
    pass "approved module present: $f"
  fi
done

if [ ! -f "$ROOT/docs/ENGINE_MODULE_MANIFEST.md" ]; then
  fail "missing docs/ENGINE_MODULE_MANIFEST.md"
else
  pass "ENGINE_MODULE_MANIFEST.md present"
fi

if [ "$FAIL" -ne 0 ]; then
  echo "audit_unconditional_sources: FAILED" >&2
  exit 1
fi

echo "audit_unconditional_sources: passed"
exit 0
