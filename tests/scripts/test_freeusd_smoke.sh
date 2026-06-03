#!/usr/bin/env bash
# Verify FreeUSD is linked when USE_FREEUSD=ON (default).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
RELEASE_DIR="${1:-$PROJECT_ROOT/release}"

VK_SO=""
for candidate in \
  "$RELEASE_DIR/idtech3_vulkan.so" \
  "$RELEASE_DIR/idtech3_vulkan_x86_64.so"; do
  if [ -f "$candidate" ]; then
    VK_SO="$candidate"
    break
  fi
done

if [ -z "$VK_SO" ]; then
  echo "skip: Vulkan renderer .so not found in $RELEASE_DIR"
  exit 0
fi

if ! grep -Fq 'R_RegisterFreeusdMesh' < <(nm -D "$VK_SO" 2>/dev/null); then
  echo "fail: $VK_SO missing R_RegisterFreeusdMesh (USE_FREEUSD=OFF or broken link?)"
  exit 1
fi

if ! grep -Fq 'R_Freeusd_Init' < <(nm -D "$VK_SO" 2>/dev/null); then
  echo "fail: $VK_SO missing R_Freeusd_Init"
  exit 1
fi

CLIENT=""
for candidate in "$RELEASE_DIR/idtech3" "$RELEASE_DIR/idtech3.x86_64"; do
  if [ -f "$candidate" ]; then
    CLIENT="$candidate"
    break
  fi
done

if [ -n "$CLIENT" ]; then
  if ! grep -Fq 'usd_info' < <(strings "$CLIENT" 2>/dev/null); then
    echo "fail: $CLIENT missing usd_info command"
    exit 1
  fi
fi

if [ ! -f "$PROJECT_ROOT/tests/data/usd/parity_geom_mesh.usda" ]; then
  echo "fail: missing tests/data/usd/parity_geom_mesh.usda"
  exit 1
fi

echo "ok: FreeUSD smoke (renderer + client symbols, test USDA present)"
